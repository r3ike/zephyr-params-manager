#!/usr/bin/env python3
import yaml
import sys
import os

def generate(schema_file, output_dir):
    with open(schema_file, 'r') as f:
        data = yaml.safe_load(f)
    
    params = data['parameters']
    
    # Genera param_defs.hpp
    with open(os.path.join(output_dir, 'param_defs.hpp'), 'w') as f:
        f.write("""#pragma once
#include <cstdint>

namespace autopilot::params {

enum class ID : uint16_t {
""")
        for i, p in enumerate(params):
            f.write(f"    {p['name']} = {i},\n")
        f.write("    COUNT\n};\n\n")
        
        f.write("struct Metadata {\n    ID id;\n    const char* name;\n    enum Type { INT32, FLOAT, BOOL } type;\n    union { int32_t i32_default; float f_default; bool b_default; };\n    float min;\n    float max;\n    const char* desc;\n};\n\n")
        f.write("extern const Metadata g_param_metadata[];\n\n")
        f.write("} // namespace autopilot::params\n")
    
    # Genera param_defs.cpp
    with open(os.path.join(output_dir, 'param_defs.cpp'), 'w') as f:
        f.write('#include "param_defs.hpp"\n\nnamespace autopilot::params {\n\n')
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
        print("Usage: generate_params.py <schema.yaml> <output_dir>")
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])