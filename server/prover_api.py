"""
ATP Research — Prover API routes.
Calls C++ binaries for propositional/FOL, Prover9 for first-order logic,
and uses Claude API to translate Prover9 proofs into human-readable language.
"""
from fastapi import APIRouter
from pydantic import BaseModel
from typing import Optional
import subprocess
import tempfile
import os
import asyncio
import json

router = APIRouter(prefix="/api/prove")

PROVER_DIR = os.path.join(os.path.dirname(__file__), "prover")


class PropositionalReq(BaseModel):
    formula: str


class FOLReq(BaseModel):
    formula: str


class Prover9Req(BaseModel):
    formulas: Optional[str] = None
    goals: Optional[str] = None
    timeout: int = 10


# ─── Propositional Logic (C++ binary) ───

@router.post("/propositional")
async def prove_propositional(body: PropositionalReq):
    if not body.formula.strip():
        return {"error": "式を入力してください"}
    binary = os.path.join(PROVER_DIR, "resolution")
    if not os.path.isfile(binary):
        return {"error": "resolution バイナリが見つかりません。make -C server/prover を実行してください。"}
    try:
        proc = await asyncio.create_subprocess_exec(
            binary, body.formula,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=15)
        output = stdout.decode("utf-8", errors="replace")
        result = json.loads(output)
        return result
    except asyncio.TimeoutError:
        return {"error": "タイムアウト: 証明が制限時間内に完了しませんでした。"}
    except json.JSONDecodeError:
        return {"error": "C++バイナリの出力を解析できません", "raw": output[:2000]}
    except Exception as e:
        return {"error": str(e)}


# ─── First-Order Logic (C++ binary) ───

@router.post("/fol")
async def prove_fol(body: FOLReq):
    if not body.formula.strip():
        return {"error": "式を入力してください"}
    binary = os.path.join(PROVER_DIR, "fol_prover")
    if not os.path.isfile(binary):
        return {"error": "fol_prover バイナリが見つかりません。make -C server/prover を実行してください。"}
    try:
        proc = await asyncio.create_subprocess_exec(
            binary, body.formula,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=15)
        output = stdout.decode("utf-8", errors="replace")
        result = json.loads(output)
        return result
    except asyncio.TimeoutError:
        return {"error": "タイムアウト: 証明が制限時間内に完了しませんでした。"}
    except json.JSONDecodeError:
        return {"error": "C++バイナリの出力を解析できません", "raw": output[:2000]}
    except Exception as e:
        return {"error": str(e)}


# ─── Prover9 (system binary + AI translation) ───

def _build_prover9_input(formulas: Optional[str], goals: Optional[str]) -> str:
    inp = ""
    if formulas:
        inp += "formulas(sos).\n"
        inp += formulas.strip() + "\n"
        inp += "end_of_list.\n\n"
    if goals:
        inp += "formulas(goals).\n"
        inp += goals.strip() + "\n"
        inp += "end_of_list.\n"
    return inp


def _parse_prover9_output(output: str):
    proved = "THEOREM PROVED" in output
    search_failed = "SEARCH FAILED" in output

    proof = ""
    import re
    m = re.search(r"={10,} PROOF ={10,}([\s\S]*?)={10,} end of proof", output)
    if m:
        proof = m.group(1).strip()

    stats = ""
    sm = re.search(r"User_CPU=[\d.]+", output)
    if sm:
        stats = sm.group(0)

    return proved, search_failed, proof, stats


async def _ai_translate_proof(proof_text: str, input_text: str) -> Optional[str]:
    """Use Claude API to translate a Prover9 proof into human-readable Japanese."""
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        return None
    try:
        import anthropic
        client = anthropic.Anthropic(api_key=api_key)
        message = client.messages.create(
            model="claude-haiku-4-5-20251001",
            max_tokens=2048,
            messages=[{
                "role": "user",
                "content": f"""以下はProver9（自動定理証明器）の証明出力です。この証明を人間が理解できる日本語で分かりやすく説明してください。

## 入力
```
{input_text}
```

## Prover9の証明出力
```
{proof_text}
```

## 指示
- 各ステップで何が起きているか簡潔に説明
- 数学の専門用語を適度に使いつつ、論理の流れが分かるように
- 最終的に何が証明されたかをまとめる
- Markdown形式で出力"""
            }]
        )
        return message.content[0].text
    except Exception as e:
        return f"(AI翻訳エラー: {e})"


@router.post("/prover9")
async def prove_prover9(body: Prover9Req):
    if not body.goals and not body.formulas:
        return {"error": "入力が必要です"}

    input_text = _build_prover9_input(body.formulas, body.goals)

    # Write to temp file
    with tempfile.NamedTemporaryFile(mode="w", suffix=".in", delete=False) as f:
        f.write(input_text)
        tmp_path = f.name

    timeout_sec = min(body.timeout, 30)

    try:
        proc = await asyncio.create_subprocess_exec(
            "prover9", "-f", tmp_path,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout_sec)
        output = stdout.decode("utf-8", errors="replace")
    except asyncio.TimeoutError:
        try:
            proc.kill()
        except Exception:
            pass
        return {
            "success": False,
            "proved": False,
            "output": "タイムアウト: 証明が制限時間内に完了しませんでした。",
            "input": input_text,
        }
    except FileNotFoundError:
        return {"error": "prover9 バイナリが見つかりません。prover9 をインストールしてください。"}
    except Exception as e:
        return {"error": str(e)}
    finally:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass

    proved, search_failed, proof, stats = _parse_prover9_output(output)

    # AI translation (run in parallel with response preparation)
    ai_explanation = None
    if proved and proof:
        ai_explanation = await _ai_translate_proof(proof, input_text)

    return {
        "success": True,
        "proved": proved,
        "searchFailed": search_failed,
        "proof": proof,
        "stats": stats,
        "output": output[:5000],
        "input": input_text,
        "aiExplanation": ai_explanation,
    }
