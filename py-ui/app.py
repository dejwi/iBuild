import os
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Optional

import llm
import multimodal
from huggingface_hub import hf_hub_download, snapshot_download
from janus.models import MultiModalityCausalLM
from tqdm.auto import tqdm


class AIApplication(multimodal.Mixin, llm.Mixin):
    def __init__(self, root):
        self.root = root
        root.title("AI Dataset Creator")

        self.create_widgets()

        # Model paths
        self.janus_model_path = "deepseek-ai/Janus-Pro-1B"
        self.llm_model_path = {
            "repo_id": "lmstudio-community/DeepSeek-R1-Distill-Qwen-7B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
        }
        self.janus_model_local_path = os.path.join("./models", self.janus_model_path)
        self.llm_model_local_path = os.path.join(
            "./models", self.llm_model_path["filename"]
        )

        # Model placeholders
        self.janus_model: MultiModalityCausalLM | None = None
        self.janus_processor: Any = None
        self.llm_model = None
        self.reasoning_tokenizer = None

        self.running = False

        self.patch_tqm()

    def create_widgets(self):
        self.prompt_label = tk.Label(self.root, text="Enter Prompt:")
        self.prompt_label.pack(pady=5)

        self.prompt_entry = tk.Entry(self.root, width=80)
        self.prompt_entry.pack(pady=5)
        self.prompt_entry.insert(
            tk.END,
            "starter house with a glowstone roof. Block palette with indexes: minecraft:air 0, minecraft:birch_planks 1, minecraft:cobblestone 2, minecraft:door 3, minecraft:glowstone 4. Make sure that the house is walkable so is at least partially filled with minecraft:air.. Dimensions of the build are x_size = 10, z_size = 10, y_size = 6.",
        )

        self.progress_label = tk.Label(self.root, text="Ready")
        self.progress_label.pack(pady=5)

        self.progress_bar = ttk.Progressbar(
            self.root, orient=tk.HORIZONTAL, length=400, mode="determinate"
        )
        self.progress_bar.pack(pady=5)

        self.generate_btn = tk.Button(
            self.root, text="Generate", command=self.start_generation
        )
        self.generate_btn.pack(pady=10)

        self.image_label = tk.Label(self.root)
        self.image_label.pack(pady=5)

        self.output_text = tk.Text(self.root, height=10, width=80)
        self.output_text.pack(pady=5)

    def start_generation(self):
        self.generate_btn.config(state=tk.DISABLED)
        self.running = True
        thread = threading.Thread(target=self.generate_process)
        thread.start()

    def generate_process(self):
        try:
            user_prompt = self.prompt_entry.get()

            # Step 1: Generate image with Janus
            image, description = self.run_janus_steps(user_prompt)
            with open("./generated_samples/desc_log.txt", "w") as f:
                f.write(description)

            # Step 2: Generate final output with R1
            final_data = self.run_llm(user_prompt, description)
            with open("./generated_samples/final_data.txt", "w") as f:
                f.write(final_data)

            self.update_progress(value=100)

        except Exception as e:
            messagebox.showerror("Error", str(e))
        finally:
            self.running = False

    def update_progress(self, message=None, value=None):
        self.root.after(0, lambda: self._update_progress(message, value))

    def _update_progress(self, message, value):
        if message is not None:
            self.progress_label.config(text=message)
        if value is not None:
            self.progress_bar["value"] = value
        self.root.update_idletasks()

    def patch_tqm(self):
        update_progress = self.update_progress
        original_update = tqdm.update

        def patched_update(self: tqdm, n=1):
            if self.n is not None and self.total is not None:
                # Format the information
                downloaded = self.n / 1024 / 1024
                total_size = self.total / 1024 / 1024
                speed = (
                    self.format_dict["rate"] / 1024 / 1024
                    if self.format_dict["rate"]
                    else 0.001
                )
                # time_left = format_time((total_size - downloaded) / speed)
                # progress_bar.progress(
                #     self.n / self.total,
                #     f"Downloaded: {int(downloaded)}/{int(total_size)} MB Speed: {speed:.2f} MB/s Remaining: {time_left}",
                # )
                update_progress(
                    f"{self.desc}: {int(downloaded)}/{int(total_size)} MB Speed: {speed:.2f} MB/s",
                    (self.n / self.total) * 100,
                )

            return original_update(self, n)

        tqdm.update = patched_update  # type: ignore

    def download_model(
        self, repo_id: str, local_dir: str, filename: Optional[str] = None
    ):
        try:
            if filename:
                return hf_hub_download(
                    repo_id=repo_id,
                    filename=filename,
                    local_dir=local_dir,
                )
            else:
                return snapshot_download(
                    repo_id=repo_id,
                    local_dir=local_dir,
                )
        except Exception as e:
            messagebox.showerror("Download Error", str(e))
            return None


if __name__ == "__main__":
    root = tk.Tk()
    app = AIApplication(root)
    root.mainloop()
