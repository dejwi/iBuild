import ctypes
import os
import re

import pyjson5


# C type definitions
class Vector3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("z", ctypes.c_int)]


class BlockBuild(ctypes.Structure):
    _fields_ = [
        ("x_size", ctypes.c_int),
        ("y_size", ctypes.c_int),
        ("z_size", ctypes.c_int),
        ("pos", Vector3),
        ("palette", ctypes.POINTER(ctypes.c_char_p)),
        ("palette_len", ctypes.c_int),
        ("indices", ctypes.POINTER(ctypes.c_int)),
    ]


# Load the shared library.
lib_path = os.path.join(os.path.dirname(__file__), "../nbt-editor/build", "libmc.so")
libmc = ctypes.CDLL(lib_path)

# Set the argument types and return type for test_chunk_edit.
libmc.test_chunk_edit.argtypes = [
    ctypes.c_char_p,  # region_path
    ctypes.c_int,  # x_chunk
    ctypes.c_int,  # z_chunk
    ctypes.POINTER(BlockBuild),  # build
]
libmc.test_chunk_edit.restype = None

# Example JSON string (you can also load this from a file)
json_str = r"""
{
  "palette": ["minecraft:air", "minecraft:cobblestone", "minecraft:oak_planks"],
  "dimensions": {
    "x_size": 10,
    "y_size": 6,
    "z_size": 10
  },
  "data": [
    [
      [2,2,2,2,2,2,2,2,2,2],
      [2,1,1,1,1,1,1,1,1,2],
[2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,2,2,2,2,2,2,2,2,2]
    ],
    [
      [2,2,2,2,2,2,2,2,2,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,1,1,1,1,1,1,1,1,2],
      [2,2,2,2,2,2,2,2,2,2]
    ]
  ]
}
"""


def clean_json(input_str):
    match = re.search(r"```json\s*([\s\S]*?)\s*```", input_str)
    if match:
        input_str = match.group(1)

    return input_str.strip()


def insert_build_save(
    ai_output: str, save_folder: str, pos_x: int, pos_y: int, pos_z: int
):
    # Parse the JSON.
    try:
        data = pyjson5.loads(clean_json(ai_output))
    except:
        raise Exception("Generated JSON was invalid")

    palette_list = data["palette"]
    indicies_list = data["data"]
    y_size = len(indicies_list)
    z_size = len(indicies_list[0])
    x_size = len(indicies_list[0][0])

    # Convert palette to a ctypes array of c_char_p.
    c_palette_arr = (ctypes.c_char_p * len(palette_list))()
    for i, item in enumerate(palette_list):
        c_palette_arr[i] = item.encode("utf-8")

    # Flatten the indices from the 3D "data" array.
    flattened_indices = [-1] * y_size * x_size * z_size
    for y in range(y_size):
        for z in range(z_size):
            for x in range(x_size):
                idx = y * x_size * z_size + z * x_size + x
                try:
                    flattened_indices[idx] = indicies_list[y][z][x]
                except IndexError:
                    flattened_indices[idx] = -1

    num_indices = len(flattened_indices)
    c_indices_arr = (ctypes.c_int * num_indices)(*flattened_indices)

    # Create an instance of BlockBuild and fill in the values.
    c_build = BlockBuild()
    c_build.x_size = x_size
    c_build.y_size = y_size
    c_build.z_size = z_size
    c_build.pos = Vector3(pos_x, pos_y, pos_z)  # Set to (0, 0, 0) or update as needed.
    c_build.palette = ctypes.cast(c_palette_arr, ctypes.POINTER(ctypes.c_char_p))
    c_build.palette_len = len(palette_list)
    c_build.indices = ctypes.cast(c_indices_arr, ctypes.POINTER(ctypes.c_int))

    # temp
    # region_path = b"/Users/dawid/Library/Application Support/PrismLauncher/instances/Light-Craft---1.19.3---4.6.3/minecraft/saves/testc/region/r.0.0.mca"
    region_folder = os.path.join(save_folder, "region")

    # Call the C function.
    # libmc.test_chunk_edit(region_path, x_chunk, z_chunk, ctypes.byref(c_build))
    chunk_start_x = c_build.pos.x // 16
    chunk_end_x = (c_build.pos.x + c_build.x_size - 1) // 16
    chunk_start_z = c_build.pos.z // 16
    chunk_end_z = (c_build.pos.z + c_build.z_size - 1) // 16

    for ch_x in range(chunk_start_x, chunk_end_x + 1):
        for ch_z in range(chunk_start_z, chunk_end_z + 1):
            region_path = os.path.join(region_folder, f"r.{ch_x >> 5}.{ch_z>>5}.mca")
            if not os.path.isfile(region_path):
                print(region_path)
                raise Exception(
                    "Region file doesn't exist. Maybe terrain didn't generate in the given area"
                )

            libmc.test_chunk_edit(
                region_path.encode("utf-8"), ch_x, ch_z, ctypes.byref(c_build)
            )
