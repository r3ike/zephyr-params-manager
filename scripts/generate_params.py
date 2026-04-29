#!/usr/bin/env python3
import yaml
import sys
import os
import glob

def collect_params(root_dir):
    """Cerca tutti i module.yaml e restituisce una lista ordinata di parametri."""
    all_params = []
    for yaml_file in sorted(glob.glob(os.path.join(root_dir, "**/module.yaml"), recursive=True)):
        with open(yaml_file, 'r') as f:
            data = yaml.safe_load(f)
        module_name = data.get('module_name', 'unknown')
        params = data.get('parameters', [])
        for group in params:
            definitions = group.get('definitions', {})
            for param_name, param_def in definitions.items():
                # Prefissa con il nome del modulo per evitare collisioni
                full_name = f"{module_name.upper()}_{param_name}"
                param_entry = {
                    'name': full_name,
                    'desc': param_def.get('description', {}).get('short', ''),
                    'type': param_def.get('type', 'float'),
                    'default': param_def.get('default', 0),
                    'min': param_def.get('min', 0),
                    'max': param_def.get('max', 0),
                }
                all_params.append(param_entry)
    return all_params

def generate(schema_root, output_dir):
    params = collect_params(schema_root)
    
    # Genera param_defs.hpp
    with open(os.path.join(output_dir, 'param_defs.hpp'), 'w') as f:
        f.write("#pragma once\n#include <cstdint>\n\n")
        f.write("namespace autopilot::params {\n\n")
        f.write("enum class ID : uint16_t {\n")
        for i, p in enumerate(params):
            f.write(f"    {p['name']} = {i},\n")
        f.write("    COUNT\n};\n\n")
        
        f.write("struct Metadata {\n")
        f.write("    ID id;\n")
        f.write("    const char* name;\n")
        f.write("    enum Type { INT32, FLOAT, BOOL } type;\n")
        f.write("    union { int32_t i32_default; float f_default; bool b_default; };\n")
        f.write("    float min;\n")
        f.write("    float max;\n")
        f.write("    const char* desc;\n")
        f.write("};\n\n")
        f.write("extern const Metadata g_param_metadata[];\n\n")
        f.write("} // namespace autopilot::params\n")
    
    # Genera param_defs.cpp
    with open(os.path.join(output_dir, 'param_defs.cpp'), 'w') as f:
        f.write('#include "param_defs.hpp"\n\n')
        f.write("namespace autopilot::params {\n\n")
        f.write("const Metadata g_param_metadata[] = {\n")
        for p in params:
            default_str = ""
            if p['type'] == 'float':
                default_str = f".f_default = {p['default']}f"
            elif p['type'] == 'int32':
                default_str = f".i32_default = {p['default']}"
            elif p['type'] == 'bool':
                default_str = f".b_default = {str(p['default']).lower()}"
            f.write(f"    {{ .id = ID::{p['name']}, .name = \"{p['name']}\", .type = Metadata::{p['type'].upper()}, {default_str}, .min = {p.get('min', 0)}, .max = {p.get('max', 0)}, .desc = \"{p['desc']}\" }},\n")
        f.write("};\n\n")
        f.write(f"static_assert(sizeof(g_param_metadata)/sizeof(g_param_metadata[0]) == static_cast<uint16_t>(ID::COUNT), \"Mismatch metadata count\");\n\n")
        f.write("} // namespace\n")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: generate_params.py <modules_dir> <output_dir>")
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])