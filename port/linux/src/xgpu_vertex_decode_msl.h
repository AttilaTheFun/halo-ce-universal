/* Compact immutable Xbox vertex input. The caller validates the version-1
 * 528-byte prefix and every referenced byte span before encoding a draw.
 * Byte loads intentionally support declarations with unaligned fields.
 * Packed normals stay integers until the original NV2A unpack operation. */
#ifndef XGPU_VERTEX_DECODE_MSL_H
#define XGPU_VERTEX_DECODE_MSL_H

static const char xgpu_vertex_decode_msl[] =
    "struct XgpuCompactVertexElement { uint reg, type, offset, stride; };\n"
    "struct XgpuCompactVertexInput { uint version, element_count, vertex_count, packed_mask;\n"
    "XgpuCompactVertexElement elements[16]; float4 fixed[16]; };\n"
    "uint xgpu_vertex_le16(device const uchar *bytes)\n{\n"
    "return uint(bytes[0]) | (uint(bytes[1]) << 8);\n}\n"
    "uint xgpu_vertex_le32(device const uchar *bytes)\n{\n"
    "return uint(bytes[0]) | (uint(bytes[1]) << 8) | (uint(bytes[2]) << 16) | (uint(bytes[3]) << 24);\n}\n"
    "float4 xgpu_vertex_decode(device const uchar *bytes, uint type)\n{\n"
    "float4 result = float4(0.0, 0.0, 0.0, 1.0);\n"
    "uint components = type == 0x72 ? 3 : type >> 4;\n"
    "for (uint component = 0; component < components; component++) {\n"
    "if ((type & 15) == 2) result[component] = as_type<float>(xgpu_vertex_le32(bytes + component * 4));\n"
    "else if ((type & 15) == 1 || (type & 15) == 5) {\n"
    "int value = int(xgpu_vertex_le16(bytes + component * 2));\n"
    "if (value >= 32768) value -= 65536;\n"
    "result[component] = (type & 15) == 1 ? (value == -32768 ? -1.0 : precise::divide(float(value), 32767.0)) : float(value);\n"
    "} else {\n"
    "uint channel = type == 0x40 && (component == 0 || component == 2) ? 2 - component : component;\n"
    "result[component] = precise::divide(float(bytes[channel]), 255.0);\n"
    "}\n}\nreturn result;\n}\n"
    "void xgpu_vertex_fetch(device const uchar *bytes, uint vertex_id,\n"
    "thread float4 *values, thread uint *packed)\n{\n"
    "device const XgpuCompactVertexInput &input = *reinterpret_cast<device const XgpuCompactVertexInput *>(bytes);\n"
    "for (uint reg = 0; reg < 16; reg++) { values[reg] = input.fixed[reg]; packed[reg] = 0; }\n"
    "for (uint index = 0; index < input.element_count; index++) {\n"
    "XgpuCompactVertexElement element = input.elements[index];\n"
    "device const uchar *source = bytes + element.offset + vertex_id * element.stride;\n"
    "if (element.type == 0x16) packed[element.reg] = xgpu_vertex_le32(source);\n"
    "else values[element.reg] = xgpu_vertex_decode(source, element.type);\n"
    "}\n}\n";

#endif
