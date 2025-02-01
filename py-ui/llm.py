import gc
import re
from typing import Any, Callable

from llama_cpp import Llama


class Mixin:
    update_progress: Callable[[str, int], Any]
    llm_model_local_path: str

    def run_llm(self, prompt, ai_description):
        self.update_progress("Init llm", 45)
        llm = Llama(
            model_path=self.llm_model_local_path,
            n_ctx=5000,
            n_threads=8,
            n_gpu_layers=8,
            verbose=False,
        )
        combined_prompt = f"Prompt: {prompt}\nDescription: {ai_description}\n"
        self.update_progress("Generate llm", 50)
        output = llm(combined_prompt, max_tokens=5000)
        text = output["choices"][0]["text"]  # type: ignore
        cleaned_text = re.sub(r".*?</think>\s*", "", text, flags=re.DOTALL).strip()

        del llm
        gc.collect()
        return cleaned_text
