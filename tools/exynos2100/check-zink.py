#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare smoke output to the documented Zink GL 2.1 minimum.

This is only a preflight: not Zink loading, GL conformance, or app compatibility.
Reference: https://docs.mesa3d.org/drivers/zink.html (2026-11-09).
"""
import argparse
from pathlib import Path
import re

EXTENSIONS = {
    "VK_KHR_maintenance1", "VK_KHR_maintenance5", "VK_KHR_create_renderpass2",
    "VK_KHR_imageless_framebuffer", "VK_KHR_timeline_semaphore", "VK_KHR_dynamic_rendering",
    "VK_EXT_custom_border_color", "VK_EXT_provoking_vertex", "VK_KHR_swapchain_mutable_format",
    "VK_EXT_border_color_swizzle", "VK_KHR_descriptor_update_template",
}
FEATURES = {"logicOp", "alphaToOne", "fillModeNonSolid", "shaderClipDistance", "independentBlend",
            "nullDescriptor", "customBorderColorWithoutFormat", "scalarBlockLayout",
            "rectangularLines", "bresenhamLines", "smoothLines", "stippledRectangularLines",
            "stippledBresenhamLines", "stippledSmoothLines"}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("log", type=Path)
    a = p.parse_args()
    text = a.log.read_text()
    exts = set(re.findall(r"(?m)^extension=(\S+)", text))
    # Promoted features still need to be queried; our driver advertises these
    # aliases too. This conservative preflight does not assume promotion alone.
    absent = sorted(EXTENSIONS - exts)
    if not ({"VK_EXT_robustness2", "VK_KHR_robustness2"} & exts): absent.append("robustness2")
    if not ({"VK_EXT_line_rasterization", "VK_KHR_line_rasterization"} & exts): absent.append("line_rasterization")
    fields = dict(re.findall(r"\b(\w+)=(\d+)\b", text))
    missing = sorted(f for f in FEATURES if fields.get(f) != "1")
    print("Missing advertised extensions:", ", ".join(absent) or "none")
    print("Missing or unqueried features:", ", ".join(missing) or "none")
    print("Preflight only. Actual Zink/GL/app execution and WSI must be tested separately.")
    raise SystemExit(bool(absent or missing))


if __name__ == "__main__":
    main()
