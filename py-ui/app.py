import multiprocessing
import os
import sys
import threading
import tkinter as tk
import traceback
from os.path import exists
from tkinter import filedialog, messagebox, ttk
from typing import Any, Optional

import llm
import multimodal
import sv_ttk
import torch
from huggingface_hub import hf_hub_download, snapshot_download
from janus.models import MultiModalityCausalLM
from nbt_link import insert_build_save
from prompts import default_dimensions_prompt
from tqdm.auto import tqdm
from utils import get_path_exc

AI_PRESETS = [
    {
        "text": "Custom",
        "subtext": "",
        "llm": {
            "repo_id": "bartowski/DeepSeek-R1-Distill-Qwen-32B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-32B-IQ2_XS.gguf",
            "cpu_threads": 10,
            "gpu_layers": 49,
        },
    },
    {
        "text": "16gb RAM | 10gb VRAM",
        "subtext": "32b IQ2_XS",
        "llm": {
            "repo_id": "bartowski/DeepSeek-R1-Distill-Qwen-32B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-32B-IQ2_XS.gguf",
            "cpu_threads": 10,
            "gpu_layers": 49,
        },
    },
    {
        "text": "16gb RAM | 8gb VRAM",
        "subtext": "32b IQ2_XS",
        "llm": {
            "repo_id": "bartowski/DeepSeek-R1-Distill-Qwen-32B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-32B-IQ2_XS.gguf",
            "cpu_threads": 10,
            "gpu_layers": 39,
        },
    },
    {
        "text": "16gb RAM | +-6gb VRAM",
        "subtext": "7b Q4_K_M - mostly bad results",
        "llm": {
            "repo_id": "lmstudio-community/DeepSeek-R1-Distill-Qwen-7B-GGUF",
            "filename": "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
            "cpu_threads": 8,
            "gpu_layers": 8,
        },
    },
]

default_save_paths = [
    "%APPDATA%\\.minecraft\\saves",
    "~/Library/Application Support/minecraft/saves",
    "~/.minecraft/saves",
]


class AIApplication(multimodal.Mixin, llm.Mixin):
    def __init__(self, root):
        self.root = root
        root.title("iBuild")

        if getattr(sys, "frozen", False):
            # Pyinstaller build
            root.iconbitmap(os.path.join(sys._MEIPASS, "icon.ico"))  # type: ignore
        else:
            root.iconbitmap(os.path.join(os.path.dirname(__file__), "../icon.ico"))

        # Set theme for non-macos
        if sys.platform != "darwin":
            sv_ttk.set_theme("dark")

        if torch.cuda.is_available():
            print("cuda")
        else:
            print("no cuda")
        self.create_widgets()

        # Model paths
        self.janus_model_path = "deepseek-ai/Janus-Pro-1B"
        self.llm_model_path = AI_PRESETS[0]["llm"]
        self.janus_model_local_path = get_path_exc(
            os.path.join("./models", self.janus_model_path)
        )
        self.llm_model_local_path = get_path_exc(
            os.path.join("./models", self.llm_model_path["filename"])
        )

        # Model placeholders
        self.janus_model: MultiModalityCausalLM | None = None
        self.janus_processor: Any = None
        self.llm_model = None
        self.reasoning_tokenizer = None

        self.running = False
        self.valid_saves_paths = []

        for path in default_save_paths:
            if exists(path):
                self.update_saves_folder(path, True)
                break

        self.patch_tqm()

    def create_widgets(self):
        # Prompt Section
        prompt_frame = ttk.LabelFrame(self.root, text="Prompt")
        prompt_frame.pack(fill="x", padx=10, pady=5)

        self.prompt_text = tk.Text(prompt_frame, height=5, width=80)
        self.prompt_text.pack(padx=5, pady=5)
        # Default prompt
        self.prompt_text.insert(
            tk.END,
            "starter house with a glowstone roof. Make sure that the house is walkable so is at least partially filled with minecraft:air.",
        )

        # Minecraft Saves Section
        saves_frame = ttk.LabelFrame(self.root, text="Minecraft Saves")
        saves_frame.pack(fill="x", padx=10, pady=5)

        btn_frame = ttk.Frame(saves_frame)
        btn_frame.pack(fill="x", padx=5, pady=5)
        self.select_saves_btn = ttk.Button(
            btn_frame, text="Select Saves Folder", command=self.user_handle_saves_folder
        )
        self.select_saves_btn.pack(side="left", padx=(0, 5))
        self.saves_folder_label = ttk.Label(btn_frame, text="No folder selected")
        self.saves_folder_label.pack(side="left")

        # Combobox for valid saves (directories that contain level.dat)
        self.saves_combo = ttk.Combobox(saves_frame, state="readonly")
        self.saves_combo.pack(fill="x", padx=5, pady=5)
        self.saves_combo["values"] = []

        # Build Insertion Toggle
        toggle_frame = ttk.Frame(saves_frame)
        toggle_frame.pack(fill="x", padx=10, pady=5)
        self.insert_into_save_val = tk.BooleanVar(value=True)
        self.insert_check = ttk.Checkbutton(
            toggle_frame,
            text="Insert build into Minecraft save",
            variable=self.insert_into_save_val,
        )
        self.insert_check.pack(side="left")

        # Build Position Section
        position_frame = ttk.LabelFrame(self.root, text="Build Position")
        position_frame.pack(fill="x", padx=10, pady=5)

        pos_inputs_frame = ttk.Frame(position_frame)
        pos_inputs_frame.pack(fill="x", padx=5, pady=5)

        ttk.Label(pos_inputs_frame, text="X:").pack(side="left", padx=(0, 5))
        self.pos_x = ttk.Entry(pos_inputs_frame, width=10)
        self.pos_x.pack(side="left", padx=(0, 10))
        self.pos_x.insert(tk.END, "14")

        ttk.Label(pos_inputs_frame, text="Y:").pack(side="left", padx=(0, 5))
        self.pos_y = ttk.Entry(pos_inputs_frame, width=10)
        self.pos_y.pack(side="left", padx=(0, 10))
        self.pos_y.insert(tk.END, "98")

        ttk.Label(pos_inputs_frame, text="Z:").pack(side="left", padx=(0, 5))
        self.pos_z = ttk.Entry(pos_inputs_frame, width=10)
        self.pos_z.pack(side="left")
        self.pos_z.insert(tk.END, "15")

        # Insert Last Build Button
        insert_button = ttk.Button(
            pos_inputs_frame, text="Insert Last Build", command=self.insert_last_build
        )
        insert_button.pack(pady=5)

        # Minecraft Blocks Selection
        blocks_frame = ttk.LabelFrame(self.root, text="Minecraft Blocks")
        blocks_frame.pack(fill="both", padx=10, pady=5)

        blocks_label = ttk.Label(
            blocks_frame,
            text="Select blocks to use. Minecraft id's separated by a space.",
        )
        blocks_label.pack(anchor="w", padx=5, pady=(5, 0))

        # Listbox with multiple selection; add default values.
        self.blocks_text = tk.Text(blocks_frame, height=4)
        self.blocks_text.pack(fill="both", padx=5, pady=5)
        self.blocks_text.insert(
            tk.END,
            "minecraft:air minecraft:oak_planks minecraft:cobblestone minecraft:glass minecraft:glowstone",
        )

        # AI Presets Section
        presets_frame = ttk.LabelFrame(self.root, text="AI Presets")
        presets_frame.pack(fill="x", padx=10, pady=5)

        presets_select_frame = ttk.Frame(presets_frame)
        presets_select_frame.pack(fill="x", padx=5, pady=5)

        preset_display = [f"{opt['text']} ({opt['subtext']})" for opt in AI_PRESETS]

        self.preset_combo = ttk.Combobox(presets_select_frame, state="readonly")
        self.preset_combo["values"] = preset_display
        self.preset_combo.current(1)
        self.preset_combo.pack(side="left", fill="x", expand=True)
        self.preset_combo.bind("<<ComboboxSelected>>", self.on_select_ai_preset)

        # Settings button
        self.preset_settings_btn = ttk.Button(
            presets_select_frame,
            text="Custom",
            width=8,
            command=self.open_settings_window,
        )
        self.preset_settings_btn.pack(side="left", padx=5)

        # Generate / progressbar
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

    def update_saves_folder(self, folder, silent=False):
        if folder:
            display_folder = folder if len(folder) <= 40 else "..." + folder[-40:]
            self.saves_folder_label.config(text=display_folder)
            valid_saves = []
            self.valid_saves_paths = []
            # Scan the folder for subdirectories containing level.dat
            for name in os.listdir(folder):
                subdir = os.path.join(folder, name)
                if os.path.isdir(subdir) and "level.dat" in os.listdir(subdir):
                    valid_saves.append(name)
                    self.valid_saves_paths.append(subdir)
            if valid_saves:
                self.saves_combo["values"] = valid_saves
                self.saves_combo.current(0)
            else:
                if not silent:
                    messagebox.showwarning(
                        "No valid saves",
                        "No valid Minecraft saves found (missing level.dat).",
                    )
                self.saves_combo["values"] = []
                self.valid_saves_paths = []

    def user_handle_saves_folder(self):
        folder = filedialog.askdirectory(title="Select Minecraft Saves Folder")
        self.update_saves_folder(folder)

    def on_select_ai_preset(self, event):
        idx = event.widget.current()
        selected = AI_PRESETS[idx]

        # Update Custom
        if idx != 0:
            AI_PRESETS[0]["llm"] = selected["llm"].copy()

        self.llm_model_path = selected["llm"]
        self.llm_model_local_path = get_path_exc(
            os.path.join("./models", self.llm_model_path["filename"])
        )

    def get_positions(self):
        try:
            x = int(self.pos_x.get())
            y = int(self.pos_y.get())
            z = int(self.pos_z.get())
            return [x, y, z]
        except:
            messagebox.showwarning(
                "Incorecct build position values",
                "Incorecct build position values",
            )
            return None

    def insert_last_build(self):
        if not self.saves_combo["values"]:
            messagebox.showwarning(
                "Error inserting build",
                "No valid saves.",
            )
            return
        if not exists(get_path_exc("./generated_samples/llm_clean.txt")):
            messagebox.showwarning(
                "Error inserting build",
                "./generated_samples/llm_clean.txt doesn't exist",
            )
            return

        pos = self.get_positions()
        if not pos:
            return

        try:
            f = open(get_path_exc("./generated_samples/llm_clean.txt"), "r")
            data = f.read()
            insert_build_save(
                data,
                self.valid_saves_paths[self.saves_combo.current()],
                pos[0],
                pos[1],
                pos[2],
            )
        except Exception as e:
            messagebox.showerror("Error inserting build", f"Error inserting build. {e}")
            traceback.print_exc()
        else:
            messagebox.showinfo("Succes", "Inserted build into save")

    def start_generation(self):
        self.generate_btn.config(state=tk.DISABLED)
        self.running = True
        thread = threading.Thread(target=self.generate_process)
        thread.daemon = True
        thread.start()

    def generate_process(self):
        if self.insert_into_save_val.get() and not self.saves_combo["values"]:
            messagebox.showwarning(
                "No valid saves",
                "No valid saves. Unselect inserting into minecraft save if you want to only generate data",
            )
            return

        try:
            self.update_progress("Verifying AI models files", 0)
            self.download_model(
                repo_id=self.janus_model_path, local_dir=self.janus_model_local_path
            )
            self.download_model(
                repo_id=self.llm_model_path["repo_id"],
                filename=self.llm_model_path["filename"],
                local_dir=get_path_exc("./models"),
            )

            user_prompt = self.prompt_text.get("1.0", "end-1c")
            blocks = self.blocks_text.get("1.0", "end-1c")

            palette_prompt = ""
            for i, block in enumerate(blocks.split(" ")):
                palette_prompt += f"{block} (index {i})\n"

            # Step 1: Generate image with Janus
            image_user_prompt = f"{user_prompt}. Use following blocks: {blocks}"
            image, description = self.run_janus_steps(image_user_prompt)

            # Step 2: Generate final output with R1
            prepared_prompt = f"User prompt:\n`{user_prompt}`\nBlock palette: \n{palette_prompt}.\n{default_dimensions_prompt}\nAI description:\n`{description}`"
            final_data = self.run_llm(
                prepared_prompt,
                description,
                self.llm_model_path["cpu_threads"],
                self.llm_model_path["gpu_layers"],
            )

            if self.insert_into_save_val.get():
                pos = self.get_positions()
                if pos is None:
                    self.update_progress(
                        "Invalid position - try 'insert last build' with different settings",
                        value=100,
                    )
                    return
                insert_build_save(
                    final_data,
                    self.valid_saves_paths[self.saves_combo.current()],
                    pos[0],
                    pos[1],
                    pos[2],
                )
                self.update_progress("Successfully inserted build into save", value=100)
            else:
                self.update_progress("Generated logs to ./generated_samples", value=100)

        except Exception as e:
            messagebox.showerror("Error", str(e))
            traceback.print_exc
            self.update_progress("Error", value=100)
        finally:
            self.running = False
            self.generate_btn.config(state=tk.NORMAL)

    def open_settings_window(self):
        # Create a new top-level window for settings
        settings_win = tk.Toplevel(self.root)
        self.settings_win = settings_win
        settings_win.title("AI Settings")
        settings_win.grab_set()  # Modal behavior

        custom_preset = AI_PRESETS[0]["llm"]

        # Multimodal AI model (readonly)
        section1 = ttk.Frame(settings_win)
        section1.pack(fill="x", padx=10, pady=5)
        ttk.Label(section1, text="Multimodal AI Model (readonly):").grid(
            row=0, column=0, sticky="w"
        )
        multimodal_entry = ttk.Entry(section1)
        multimodal_entry.insert(tk.END, self.janus_model_path)
        multimodal_entry.configure(state="readonly")
        multimodal_entry.grid(row=0, column=1, sticky="ew")
        section1.columnconfigure(1, weight=1)

        ttk.Separator(settings_win, orient=tk.HORIZONTAL).pack(
            fill="x", padx=10, pady=5
        )

        # LLM Model settings (repo_id and filename)
        section2 = ttk.Frame(settings_win)
        section2.pack(fill="x", padx=10, pady=5)

        ttk.Label(section2, text="LLM Model Repo ID:").grid(row=0, column=0, sticky="w")
        self.llm_repo_entry = ttk.Entry(section2)
        self.llm_repo_entry.grid(row=0, column=1, sticky="ew")
        self.llm_repo_entry.insert(tk.END, custom_preset["repo_id"])

        ttk.Label(section2, text="LLM Model Filename:").grid(
            row=1, column=0, sticky="w"
        )
        self.llm_filename_entry = ttk.Entry(section2)
        self.llm_filename_entry.grid(row=1, column=1, sticky="ew")
        self.llm_filename_entry.insert(tk.END, custom_preset["filename"])
        section2.columnconfigure(1, weight=1)

        ttk.Separator(settings_win, orient=tk.HORIZONTAL).pack(
            fill="x", padx=10, pady=5
        )

        # Other AI Settings
        section3 = ttk.Frame(settings_win)
        section3.pack(fill="x", padx=10, pady=5)

        ttk.Label(section3, text="GPU Layers:").grid(row=1, column=0, sticky="w")
        self.gpu_layers_entry = ttk.Entry(section3)
        self.gpu_layers_entry.grid(row=1, column=1, sticky="ew")
        self.gpu_layers_entry.insert(tk.END, custom_preset["gpu_layers"])

        ttk.Label(section3, text="CPU Threads:").grid(row=2, column=0, sticky="w")
        self.cpu_threads_entry = ttk.Entry(section3)
        self.cpu_threads_entry.grid(row=2, column=1, sticky="ew")
        self.cpu_threads_entry.insert(tk.END, custom_preset["cpu_threads"])
        section3.columnconfigure(1, weight=1)

        # Save button
        btn_frame = ttk.Frame(settings_win)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Save", command=self.save_custom_settings).pack()

    def save_custom_settings(self):
        gpu_layers = self.gpu_layers_entry.get()
        cpu_threads = self.cpu_threads_entry.get()
        repo_id = self.llm_repo_entry.get()
        filename = self.llm_filename_entry.get()

        self.preset_combo.current(0)

        # Update custom preset
        AI_PRESETS[0]["llm"] = {
            "repo_id": repo_id,
            "filename": filename,
            "cpu_threads": cpu_threads,
            "gpu_layers": gpu_layers,
        }

        self.llm_model_path = AI_PRESETS[0]["llm"]
        self.llm_model_local_path = get_path_exc(
            os.path.join("./models", self.llm_model_path["filename"])
        )

        self.settings_win.destroy()

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
                desc = self.desc if "desc" in self.__dict__ else "Downloading"
                update_progress(
                    f"{desc}: {int(downloaded)}/{int(total_size)} MB Speed: {speed:.2f} MB/s",
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
    multiprocessing.freeze_support()
    root = tk.Tk()
    app = AIApplication(root)
    root.mainloop()
