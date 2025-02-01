import os
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
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
        # ----- Prompt Section -----
        prompt_frame = ttk.LabelFrame(self.root, text="Prompt")
        prompt_frame.pack(fill="x", padx=10, pady=5)

        self.prompt_text = tk.Text(prompt_frame, height=5, width=80)
        self.prompt_text.pack(padx=5, pady=5)
        # Insert default text
        self.prompt_text.insert(
            tk.END,
            "starter house with a glowstone roof. Block palette with indexes: minecraft:air 0, minecraft:birch_planks 1, minecraft:cobblestone 2, minecraft:door 3, minecraft:glowstone 4. Make sure that the house is walkable so is at least partially filled with minecraft:air..\nDimensions of the build are x_size = 10, z_size = 10, y_size = 6.",
        )

        # ----- Minecraft Saves Section -----
        saves_frame = ttk.LabelFrame(self.root, text="Minecraft Saves")
        saves_frame.pack(fill="x", padx=10, pady=5)

        btn_frame = ttk.Frame(saves_frame)
        btn_frame.pack(fill="x", padx=5, pady=5)
        self.select_saves_btn = ttk.Button(
            btn_frame, text="Select Saves Folder", command=self.select_saves_folder
        )
        self.select_saves_btn.pack(side="left", padx=(0, 5))
        self.saves_folder_label = ttk.Label(btn_frame, text="No folder selected")
        self.saves_folder_label.pack(side="left")

        # Combobox for valid saves (directories that contain level.dat)
        self.saves_combo = ttk.Combobox(saves_frame, state="readonly")
        self.saves_combo.pack(fill="x", padx=5, pady=5)
        self.saves_combo["values"] = []  # initially empty

        # ----- Build Insertion Toggle -----
        toggle_frame = ttk.Frame(saves_frame)
        toggle_frame.pack(fill="x", padx=10, pady=5)
        self.insert_into_save = tk.BooleanVar(value=True)
        self.insert_check = ttk.Checkbutton(
            toggle_frame,
            text="Insert build into Minecraft save",
            variable=self.insert_into_save,
        )
        self.insert_check.pack(side="left")

        # ----- Minecraft Blocks Selection -----
        blocks_frame = ttk.LabelFrame(self.root, text="Minecraft Blocks")
        blocks_frame.pack(fill="both", padx=10, pady=5)

        blocks_label = ttk.Label(
            blocks_frame,
            text="Select blocks to use in the build (Ctrl+Click to select multiple):",
        )
        blocks_label.pack(anchor="w", padx=5, pady=(5, 0))

        # Listbox with multiple selection; add default values.
        self.blocks_listbox = tk.Listbox(blocks_frame, selectmode=tk.MULTIPLE, height=6)
        self.blocks_listbox.pack(fill="both", padx=5, pady=5)
        available_blocks = [
            "minecraft:air",
            "minecraft:oak_planks",
            "minecraft:glass",
            "minecraft:stone",
            "minecraft:brick_block",
            "minecraft:diamond_block",
        ]
        for block in available_blocks:
            self.blocks_listbox.insert(tk.END, block)
        # Preselect the defaults
        for default in ["minecraft:air", "minecraft:oak_planks", "minecraft:glass"]:
            try:
                index = available_blocks.index(default)
                self.blocks_listbox.select_set(index)
            except ValueError:
                pass

        # ----- AI Presets Section -----
        presets_frame = ttk.LabelFrame(self.root, text="AI Presets")
        presets_frame.pack(fill="x", padx=10, pady=5)

        presets_select_frame = ttk.Frame(presets_frame)
        presets_select_frame.pack(fill="x", padx=5, pady=5)

        self.preset_options = [
            {"text": "24gb RAM, 10gb GB", "subtext": "recommended"},
            {"text": "16gb RAM, 8gb GB", "subtext": "balanced"},
            {"text": "8gb RAM, 4gb GB", "subtext": "low memory"},
        ]
        # We combine text and subtext for display in the dropdown.
        preset_display = [
            f"{opt['text']} ({opt['subtext']})" for opt in self.preset_options
        ]

        self.preset_combo = ttk.Combobox(presets_select_frame, state="readonly")
        self.preset_combo["values"] = preset_display
        self.preset_combo.current(0)
        self.preset_combo.pack(side="left", fill="x", expand=True)

        # Settings button with a gear icon (using Unicode gear symbol)
        # self.preset_settings_btn = ttk.Button(presets_select_frame, text="?", width=3, command=self.open_settings_window)
        self.preset_settings_btn = ttk.Button(
            presets_select_frame, text="Custom", width=3
        )
        self.preset_settings_btn.pack(side="left", padx=5)

        # ----- Generation & Output Section -----
        action_frame = ttk.Frame(self.root)
        action_frame.pack(fill="both", padx=10, pady=10)

        self.generate_btn = ttk.Button(
            action_frame, text="Generate", command=self.start_generation
        )
        self.generate_btn.pack(pady=5)

        self.progress_label = ttk.Label(action_frame, text="Ready")
        self.progress_label.pack(pady=5)

        self.progress_bar = ttk.Progressbar(
            action_frame, orient=tk.HORIZONTAL, length=400, mode="determinate"
        )
        self.progress_bar.pack(pady=5)

        self.output_text = tk.Text(action_frame, height=10, width=80)
        self.output_text.pack(pady=5)

        self.image_label = ttk.Label(action_frame)
        self.image_label.pack(pady=5)

    def select_saves_folder(self):
        folder = filedialog.askdirectory(title="Select Minecraft Saves Folder")

        if folder:
            self.saves_folder_label.config(text=folder)
            valid_saves = []
            # Scan the folder for subdirectories containing level.dat
            for name in os.listdir(folder):
                subdir = os.path.join(folder, name)
                if os.path.isdir(subdir) and "level.dat" in os.listdir(subdir):
                    valid_saves.append(name)
            if valid_saves:
                self.saves_combo["values"] = valid_saves
                self.saves_combo.current(0)
            else:
                messagebox.showwarning(
                    "No valid saves",
                    "No valid Minecraft saves found (missing level.dat).",
                )
                self.saves_combo["values"] = []

    def start_generation(self):
        self.generate_btn.config(state=tk.DISABLED)
        self.running = True
        thread = threading.Thread(target=self.generate_process)
        thread.start()

    def generate_process(self):
        try:
            user_prompt = self.prompt_text.get("1.0", "end-1c")

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
