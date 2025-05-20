#!/usr/bin/env python3

import argparse
import dataclasses
import enum
import itertools
import json
from pathlib import Path


@dataclasses.dataclass
class ShaderJsonMetadata:
    num_samplers: int
    num_storage_textures: int
    num_storage_buffers: int
    num_uniform_buffers: int

@dataclasses.dataclass
class ShaderMetadata:
    name: str
    variable: str
    json: ShaderJsonMetadata

class ShaderFormat(enum.StrEnum):
    SPIRV = "SDL_GPU_SHADERFORMAT_SPIRV"
    DXIL = "SDL_GPU_SHADERFORMAT_DXIL"
    MSL = "SDL_GPU_SHADERFORMAT_MSL"

class ShaderStage(enum.StrEnum):
    VERTEX = "SDL_GPU_SHADERSTAGE_VERTEX"
    FRAGMENT = "SDL_GPU_SHADERSTAGE_FRAGMENT"

STR_TO_SHADERSTAGE = {
    "vertex": ShaderStage.VERTEX,
    "fragment": ShaderStage.FRAGMENT,
}


def write_generated_warning(f: "io") -> None:
    f.write("// +==============================================+\n")
    f.write("// | This file is auto-generated, do not edit it. |\n")
    f.write("// +==============================================+\n")
    f.write("\n")

def write_uint8_array(f: "io", variable: str, data: bytes) -> None:
    f.write(f"static const Uint8 {variable}[{len(data)}] = {{\n")
    for rowdata in itertools.batched(data, 16):
        f.write("  "),
        f.write(", ".join(f"0x{d:02x}" for d in rowdata))
        f.write(",\n")
    f.write(f"}};\n")

def write_gpushadercreateinfo(f: "io", shader_metadata: ShaderMetadata, entrypoint: str, suffix: str, format: ShaderFormat, stage: ShaderStage):
    f.write(f"  {{\n")
    f.write(f"    /* code_size */             sizeof({shader_metadata.variable}{suffix}),\n")
    f.write(f"    /* code */                  {shader_metadata.variable}{suffix},\n")
    f.write(f"    /* entrypoint */            \"{entrypoint}\",\n")
    f.write(f"    /* format */                {format},\n")
    f.write(f"    /* stage */                 {stage},\n")
    f.write(f"    /* num_samplers */          {shader_metadata.json.num_samplers},\n")
    f.write(f"    /* num_storage_textures */  {shader_metadata.json.num_storage_textures},\n")
    f.write(f"    /* num_storage_buffers */   {shader_metadata.json.num_storage_buffers},\n")
    f.write(f"    /* num_uniform_buffers */   {shader_metadata.json.num_uniform_buffers},\n")
    f.write(f"  }},\n")

def write_shader_switch(f: "io", suffix: str, capitalized_stage: str, shader_var_names: list[tuple[str, str]]) -> None:
    f.write(f"    switch (id) {{\n")
    for name, var_name in shader_var_names:
        f.write(f"    case {capitalized_stage}ShaderId::{name}:\n")
        f.write(f"      *size = SDL_arraysize({var_name}{suffix});\n")
        f.write(f"      return {var_name}{suffix};\n")
    f.write(f"    default:\n")
    f.write(f"      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, \"Invalid {capitalized_stage} shader id: %d\", id);\n")
    f.write(f"      break;\n")
    f.write(f"    }}\n")

def main() -> None:
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.set_defaults(subparser=None)
    subparsers = parser.add_subparsers()

    header_parser = subparsers.add_parser("header")
    header_parser.add_argument("--stage", choices=("vertex", "fragment"), required=True)
    header_parser.add_argument("--variable", required=True)
    header_parser.add_argument("--dxil", type=Path, required=True)
    header_parser.add_argument("--msl", type=Path, required=True)
    header_parser.add_argument("--spirv", type=Path, required=True)
    header_parser.add_argument("--output", "-o", type=Path, required=True)
    header_parser.set_defaults(subparser="header")

    index_parser = subparsers.add_parser("index")
    index_parser.add_argument("--output", type=Path, required=True)
    index_parser.add_argument("--header", type=Path, required=True)
    index_parser.add_argument("--shader-headers", nargs="+", type=Path, required=True)
    index_parser.add_argument("--shader-names", nargs="+", type=str, required=True)
    index_parser.add_argument("--shader-variables", nargs="+", type=str, required=True)
    index_parser.add_argument("--shader-stages", nargs="+", type=str, choices=("vertex", "fragment"), required=True)
    index_parser.add_argument("--shader-jsons", nargs="+", type=Path, required=True)
    index_parser.set_defaults(subparser="index")

    args = parser.parse_args()
    if args.subparser == "header":
        variable = args.variable
        if not variable.isidentifier():
            parser.error(f"--variable must get an identifier ({variable})")

        with args.output.open("w", newline="\n") as f:
            f.write("#pragma once\n")
            f.write("\n")
            f.write("// clang-format off\n")
            f.write("\n")
            write_generated_warning(f)
            f.write("#include <SDL3/SDL.h>\n")
            f.write("\n")
            f.write("// DXIL only makes sense on Windows platforms\n")
            f.write("#if defined(SDL_PLATFORM_WINDOWS)\n")
            write_uint8_array(f=f, data=args.dxil.read_bytes(), variable=variable + "_dxil")
            f.write("#endif\n")
            f.write("\n")
            f.write("// MSL only makes sense on Apple platforms\n")
            f.write("#if defined(SDL_PLATFORM_APPLE)\n")
            write_uint8_array(f=f, data=args.msl.read_bytes(), variable=variable + "_msl")
            f.write("#endif\n")
            f.write("\n")
            write_uint8_array(f=f, data=args.spirv.read_bytes(), variable=variable + "_spirv")
    elif args.subparser == "index":
        if len({len(args.shader_headers), len(args.shader_names), len(args.shader_variables), len(args.shader_stages), len(args.shader_jsons)}) != 1:
            parser.error("--shader-headers, --shader-names, --shader-variables and --shader-stages must get same number of arguments")

        stage_metadatas: dict[str,ShaderMetadata] = {
            "vertex": [],
            "fragment": [],
        }
        for str_stage, name, variable, json_path in zip(args.shader_stages, args.shader_names, args.shader_variables, args.shader_jsons):
            try:
                json_metadata = ShaderJsonMetadata(**json.loads(json_path.read_text()))
            except IOError:
                parser.error(f"Failed to open/read {json_path}")
            stage_metadatas.get(str_stage).append(ShaderMetadata(name=name, variable=variable, json=json_metadata))
        with args.header.open("w", newline="\n") as f:
            f.write("#pragma once\n")
            f.write("\n")
            f.write("// clang-format off\n")
            f.write("\n")
            write_generated_warning(f)
            f.write("#include <SDL3/SDL.h>\n")
            f.write("\n")
            for str_stage, shader_metadatas in stage_metadatas.items():
                if not shader_metadatas:
                    continue
                capitalized_stage = str_stage.capitalize()
                f.write(f"enum {capitalized_stage}ShaderId {{\n")
                for shader_metadata in shader_metadatas:
                    f.write(f"  {shader_metadata.name},\n")
                f.write("};\n\n")
            for str_stage, shader_metadatas in stage_metadatas.items():
                if not shader_metadatas:
                    continue
                capitalized_stage = str_stage.capitalize()
                f.write(f"const SDL_GPUShaderCreateInfo* Get{capitalized_stage}ShaderCode({capitalized_stage}ShaderId id, SDL_GPUShaderFormat formats);\n\n")
        with args.output.open("w", newline="\n") as f:
            f.write("// clang-format off\n")
            f.write("\n")
            write_generated_warning(f)
            f.write(f"#include \"{args.header.name}\"\n\n")
            for shader_header in args.shader_headers:
                f.write(f"#include \"{shader_header.name}\"\n")

            for str_stage, shader_metadatas in stage_metadatas.items():
                if not shader_metadatas:
                    continue
                capitalized_stage = str_stage.capitalize()
                stage = STR_TO_SHADERSTAGE[str_stage]
                f.write("\n")
                f.write("#if defined(SDL_PLATFORM_WINDOWS)\n")
                f.write(f"static const SDL_GPUShaderCreateInfo {capitalized_stage}ShaderDXILCodes[] = {{\n")
                f.write(f"  // {capitalized_stage}ShaderId::{shader_metadata.name}\n")
                for shader_metadata in shader_metadatas:
                    write_gpushadercreateinfo(f, shader_metadata, entrypoint="main", suffix="_dxil", format=ShaderFormat.DXIL, stage=stage)
                f.write(f"}};\n")
                f.write("#endif\n")
                f.write("\n")
                f.write("#if defined(SDL_PLATFORM_APPLE)\n")
                f.write(f"static const SDL_GPUShaderCreateInfo {capitalized_stage}ShaderMSLCodes[] = {{\n")
                for shader_metadata in shader_metadatas:
                    write_gpushadercreateinfo(f, shader_metadata, entrypoint="main0", suffix="_msl", format=ShaderFormat.MSL, stage=stage)
                f.write(f"}};\n")
                f.write("#endif\n")
                f.write("\n")
                f.write(f"static const SDL_GPUShaderCreateInfo {capitalized_stage}ShaderSPIRVCodes[] = {{\n")
                for shader_metadata in shader_metadatas:
                    write_gpushadercreateinfo(f, shader_metadata, entrypoint="main", suffix="_spirv", format=ShaderFormat.SPIRV, stage=stage)
                f.write(f"}};\n")
            for stage, shader_metadatas in stage_metadatas.items():
                if not shader_metadatas:
                    continue
                capitalized_stage = stage.capitalize()
                f.write("\n")
                f.write(f"const SDL_GPUShaderCreateInfo* Get{capitalized_stage}ShaderCode({capitalized_stage}ShaderId id, SDL_GPUShaderFormat formats) {{\n")
                f.write(f"#if defined(SDL_PLATFORM_WINDOWS)\n")
                f.write(f"  if (formats & SDL_GPU_SHADERFORMAT_DXIL) {{\n")
                f.write(f"    SDL_assert(id < SDL_arraysize({capitalized_stage}ShaderDXILCodes));\n")
                f.write(f"    return &{capitalized_stage}ShaderDXILCodes[id];\n")
                f.write(f"  }}\n")
                f.write(f"#endif\n")
                f.write(f"#if defined(SDL_PLATFORM_APPLE)\n")
                f.write(f"  if (formats & SDL_GPU_SHADERFORMAT_MSL) {{\n")
                f.write(f"    SDL_assert(id < SDL_arraysize({capitalized_stage}ShaderMSLCodes));\n")
                f.write(f"    return &{capitalized_stage}ShaderMSLCodes[id];\n")
                f.write(f"  }}\n")
                f.write(f"#endif\n")
                f.write(f"  if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {{\n")
                f.write(f"    SDL_assert(id < SDL_arraysize({capitalized_stage}ShaderSPIRVCodes));\n")
                f.write(f"    return &{capitalized_stage}ShaderSPIRVCodes[id];\n")
                f.write(f"  }}\n")
                f.write(f"  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, \"Could not find {stage} shader code for id=%d and formats=0x%x\", id, formats);\n")
                f.write(f"  return nullptr;\n")
                f.write(f"}}\n")
    else:
        parser.error("Invalid arguments")


if __name__ == "__main__":
    raise SystemExit(main())
