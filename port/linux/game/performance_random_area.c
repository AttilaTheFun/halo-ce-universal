/* A competitive match draws the darker teammate respawn area. When the
matching competitive map places a randoms mesh on the player starts, that
mesh is used. Prisoner and the other maps that never place one get a sheet
on the floor around the outer starts of each group. A classic game draws
neither. */

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "main/main.h"
#include "math/real_math.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer_model_types.h"
#include "rasterizer/rasterizer_models.h"
#include "render/render.h"
#include "scenario/scenario_definitions.h"
#include "shaders/shader_definitions.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

#define AREA_TAG_BASE 0x40440000UL
#define AREA_TAG_MAXIMUM 0x800000UL
#define AREA_VERTICES_MAXIMUM 12288
#define AREA_TRIANGLES_MAXIMUM 40000
#define AREA_PARTS_MAXIMUM 4
#define AREA_PLACES_MAXIMUM 2
#define AREA_STATE_NONE 0
#define AREA_STATE_FAILED 1
#define AREA_STATE_READY 2
#define AREA_STATE_LOADED 3
#define AREA_OUTLINE_POINTS 256
#define AREA_OUTLINE_VERTICES 1024
#define AREA_OUTLINE_TRIANGLES 1024
/* Starts this close, and on the same floor, are one respawn area. */
#define AREA_OUTLINE_LINK 4.0f
#define AREA_OUTLINE_LINK_Z 0.75f
/* The edge sits just outside the outer starts, as hangemhigh's mesh does. */
#define AREA_OUTLINE_MARGIN 0.45f
/* Just clear of the floor the spawn stands on, so the sheet does not
fight the ground it darkens. */
#define AREA_SURFACE_LIFT 0.05f

typedef char area_pointer_is_tag_sized[sizeof(void *) == 4 ? 1 : -1];
typedef char area_bitmap_is_tag_sized[sizeof(struct bitmap_data) == 0x30 ? 1 : -1];
typedef char area_vertex_is_model_sized[sizeof(struct model_vertex_uncompressed) == 0x44 ? 1 : -1];

struct area_place
{
	real_point3d position;
	real yaw;
	real pitch;
	real roll;
};

struct area_part
{
	struct vertex_buffer vertices;
	struct triangle_buffer triangles;
	long triangle_count;
	real_point3d centroid;
	real radius;
};

static int area_state;
static int area_logged;
static int area_authored_tried;
static int area_part_count;
static int area_place_count;
static int area_bounds_set;
static real area_min_x, area_min_y, area_min_z;
static real area_max_x, area_max_y, area_max_z;
static real_point3d area_offset;
static struct area_place area_places[AREA_PLACES_MAXIMUM];
static struct area_part area_parts[AREA_PARTS_MAXIMUM];
static real_point3d area_node_translation;
static struct bitmap_group area_bitmap_group;
static struct bitmap_data area_bitmap;
static unsigned char *area_pixels;
static long area_pixel_bytes;
static unsigned char area_shader[0x6C] __attribute__((aligned(8)));
static unsigned char area_shader_map[0xDC] __attribute__((aligned(8)));

/* The game's free halts on a null pointer. Reset runs before a volume exists. */
static void area_free(void *pointer)
{
	if (pointer)
		free(pointer);
}

static unsigned long area_u32(unsigned char const *bytes)
{
	return (unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) |
		((unsigned long)bytes[2] << 16) | ((unsigned long)bytes[3] << 24);
}

static long area_s32(unsigned char const *bytes)
{
	return (long)area_u32(bytes);
}

static short area_s16(unsigned char const *bytes)
{
	return (short)((unsigned short)bytes[0] | ((unsigned short)bytes[1] << 8));
}

static float area_f32(unsigned char const *bytes)
{
	float value;

	memcpy(&value, bytes, sizeof(value));
	return value;
}

static int area_span(unsigned long offset, unsigned long size, unsigned long limit)
{
	return size > 0 && offset < limit && size <= limit - offset;
}

static int area_tag_place(unsigned long virtual_address, unsigned long tag_size, unsigned long *offset)
{
	if (virtual_address < AREA_TAG_BASE)
		return 0;
	*offset = virtual_address - AREA_TAG_BASE;
	return *offset < tag_size;
}

static int area_name_is(unsigned char const *tag, unsigned long tag_size,
	unsigned long virtual_address, char const *name)
{
	unsigned long offset;
	unsigned long length = 0;
	unsigned long expect;

	if (!name || !area_tag_place(virtual_address, tag_size, &offset))
		return 0;
	expect = (unsigned long)strlen(name);
	while (offset + length < tag_size && tag[offset + length] && length < 256)
		length++;
	if (offset + length >= tag_size || tag[offset + length] || length != expect)
		return 0;
	return memcmp(tag + offset, name, length) == 0;
}

static int area_copy_name(unsigned char const *tag, unsigned long tag_size,
	unsigned long virtual_address, char *out, unsigned long out_size)
{
	unsigned long offset;
	unsigned long length = 0;

	if (!out || out_size < 2 || !area_tag_place(virtual_address, tag_size, &offset))
		return 0;
	while (offset + length < tag_size && tag[offset + length] && length + 1 < out_size)
		length++;
	if (offset + length >= tag_size || tag[offset + length] || length == 0)
		return 0;
	memcpy(out, tag + offset, length);
	out[length] = 0;
	return 1;
}

static int area_has_randoms(char const *text)
{
	unsigned long index;

	if (!text)
		return 0;
	for (index = 0; text[index]; index++)
	{
		if (strncmp(text + index, "randoms", 7) == 0)
			return 1;
	}
	return 0;
}

static int area_find(unsigned char const *tag, unsigned long tag_size,
	char const *group, char const *name, unsigned long *address)
{
	unsigned long table;
	unsigned long count;
	unsigned long index;

	if (!group || tag_size < 0x28)
		return 0;
	count = area_u32(tag + 0x0C);
	if (count == 0 || count > 65535 || !area_tag_place(area_u32(tag), tag_size, &table) ||
		!area_span(table, count * 0x20UL, tag_size))
		return 0;
	for (index = 0; index < count; index++)
	{
		unsigned char const *instance = tag + table + index * 0x20UL;

		if (instance[0] != group[0] || instance[1] != group[1] ||
			instance[2] != group[2] || instance[3] != group[3])
			continue;
		if (name && !area_name_is(tag, tag_size, area_u32(instance + 0x10), name))
			continue;
		*address = area_u32(instance + 0x14);
		return 1;
	}
	return 0;
}

static int area_leaf(char *leaf, unsigned long leaf_size)
{
	char const *name = main_get_multiplayer_map_name();
	char const *start;
	unsigned long length;
	unsigned long index;

	if (!name || !leaf || leaf_size < 2)
		return 0;
	start = name;
	for (index = 0; name[index]; index++)
	{
		if (name[index] == '\\' || name[index] == '/')
			start = name + index + 1;
	}
	length = (unsigned long)strlen(start);
	if (length < 1 || length + 1 > leaf_size)
		return 0;
	for (index = 0; index < length; index++)
	{
		char character = start[index];

		if (!((character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') ||
			character == '_' || character == '-'))
			return 0;
	}
	memcpy(leaf, start, length);
	leaf[length] = 0;
	return 1;
}

static int area_part_fits(unsigned char const *part, long *vertices, long *triangles)
{
	long verts = area_s32(part + 0x58);
	long tris = area_s32(part + 0x48);

	if (verts < 3 || verts > AREA_VERTICES_MAXIMUM ||
		tris < 1 || tris > AREA_TRIANGLES_MAXIMUM ||
		area_s16(part + 0x44) != 1 ||
		area_s16(part + 0x54) != 4)
		return 0;
	*vertices = verts;
	*triangles = tris;
	return 1;
}

static void area_shader_set(long bitmap_tag)
{
	long count = 1;
	void *maps = area_shader_map;
	short type = 6;
	short blend = 1;
	unsigned long group = BITMAP_GROUP_TAG;
	float one = 1.0f;

	/* Chicago is type 6. Multiply is blend 1, and the engine's combiner
	already inverts the fade for that blend, so the volume darkens instead
	of drawing white. */
	memset(area_shader, 0, sizeof(area_shader));
	memset(area_shader_map, 0, sizeof(area_shader_map));
	memcpy(area_shader + 0x24, &type, sizeof(type));
	/* Two sided. The sheet lies on the floor, and a one-sided face is
	gone when the camera is on the other side of it. */
	area_shader[0x29] = 1u << 2;
	memcpy(area_shader + 0x2C, &blend, sizeof(blend));
	memcpy(area_shader + 0x54, &count, sizeof(count));
	memcpy(area_shader + 0x58, &maps, sizeof(maps));
	memcpy(area_shader_map + 0x54, &one, sizeof(one));
	memcpy(area_shader_map + 0x58, &one, sizeof(one));
	memcpy(area_shader_map + 0x6C, &group, sizeof(group));
	memcpy(area_shader_map + 0x78, &bitmap_tag, sizeof(bitmap_tag));
}

static int area_bitmap_make(void)
{
	long bitmap_tag;
	long index;
	unsigned long pixel_bytes = 4UL * 4UL * 4UL;

	/* The authored bitmaps are compressed. This path only accepts the
	uncompressed 32-bit texels the pads use. A flat grey is the treadmark
	average those volumes multiply by. */
	area_pixels = (unsigned char *)malloc(pixel_bytes);
	if (!area_pixels)
		return 0;
	for (index = 0; index < (long)pixel_bytes; index += 4)
	{
		area_pixels[index] = 0x6B;
		area_pixels[index + 1] = 0x6B;
		area_pixels[index + 2] = 0x6B;
		area_pixels[index + 3] = 0xFF;
	}
	area_pixel_bytes = (long)pixel_bytes;
	memset(&area_bitmap, 0, sizeof(area_bitmap));
	area_bitmap.signature = BITMAP_GROUP_TAG;
	area_bitmap.width = 4;
	area_bitmap.height = 4;
	area_bitmap.depth = 1;
	area_bitmap.type = 0;
	area_bitmap.format = 10;
	/* Bit 0 is the power-of-two flag. A 4x4 texel is one, and the texture
	cache uses that bit when it builds the hardware copy. */
	area_bitmap.flags = 1;
	area_bitmap.pixels_size = area_pixel_bytes;
	area_bitmap.tag_index = NONE;
	area_bitmap.cache_block_index = NONE;
	memset(&area_bitmap_group, 0, sizeof(area_bitmap_group));
	area_bitmap_group.bitmaps.count = 1;
	area_bitmap_group.bitmaps.address = &area_bitmap;
	bitmap_tag = cache_files_register_runtime_ui_tag(BITMAP_GROUP_TAG,
		"scenery\\randoms\\bitmaps\\competitive_random_area", &area_bitmap_group);
	if (bitmap_tag == NONE)
		return 0;
	texture_cache_bitmap_new(bitmap_tag, &area_bitmap);
	if (area_bitmap.pixels_size != area_pixel_bytes || area_bitmap.pixels_offset != 0)
		return 0;
	area_shader_set(bitmap_tag);
	return 1;
}

static void area_discard(void)
{
	int index;

	for (index = 0; index < AREA_PARTS_MAXIMUM; index++)
	{
		rasterizer_vertex_buffer_delete(&area_parts[index].vertices);
		rasterizer_triangle_buffer_delete(&area_parts[index].triangles);
	}
	memset(area_parts, 0, sizeof(area_parts));
	area_free(area_pixels);
	area_pixels = NULL;
	area_pixel_bytes = 0;
	area_part_count = 0;
	area_place_count = 0;
	area_bounds_set = 0;
	area_offset.x = area_offset.y = area_offset.z = 0.0f;
}

static int area_load_part(FILE *file, unsigned long file_size, unsigned long vertex_file,
	unsigned long index_relative, unsigned char const *part)
{
	struct model_vertex_uncompressed *uncompressed = NULL;
	struct model_vertex_compressed *compressed = NULL;
	unsigned short *indices = NULL;
	struct area_part *slot;
	long vertex_count = 0;
	long triangle_count = 0;
	long index;
	unsigned long vertex_at;
	unsigned long index_at;
	real radius_squared = 0.0f;
	int loaded = 0;

	if (area_part_count >= AREA_PARTS_MAXIMUM)
		return 1;
	if (!area_part_fits(part, &vertex_count, &triangle_count))
		return 1;
	if (vertex_file > file_size || index_relative > file_size - vertex_file ||
		area_u32(part + 0x64) > file_size - vertex_file ||
		area_u32(part + 0x4C) > file_size - (vertex_file + index_relative))
		goto done;
	vertex_at = vertex_file + area_u32(part + 0x64);
	index_at = vertex_file + index_relative + area_u32(part + 0x4C);
	if (!area_span(vertex_at, (unsigned long)vertex_count * 0x44UL, file_size) ||
		!area_span(index_at, (unsigned long)(triangle_count + 2) * 2UL, file_size))
		goto done;
	uncompressed = (struct model_vertex_uncompressed *)malloc(
		(unsigned long)vertex_count * sizeof(*uncompressed));
	compressed = (struct model_vertex_compressed *)malloc(
		(unsigned long)vertex_count * sizeof(*compressed));
	indices = (unsigned short *)malloc((unsigned long)(triangle_count + 2) * sizeof(*indices));
	if (!uncompressed || !compressed || !indices ||
		fseek(file, (long)vertex_at, SEEK_SET) != 0 ||
		fread(uncompressed, sizeof(*uncompressed), (size_t)vertex_count, file) != (size_t)vertex_count ||
		fseek(file, (long)index_at, SEEK_SET) != 0 ||
		fread(indices, sizeof(*indices), (size_t)(triangle_count + 2), file) != (size_t)(triangle_count + 2))
		goto done;
	slot = &area_parts[area_part_count];
	slot->centroid.x = slot->centroid.y = slot->centroid.z = 0.0f;
	for (index = 0; index < vertex_count; index++)
	{
		uncompressed[index].nodes[0] = 0;
		uncompressed[index].nodes[1] = 0;
		uncompressed[index].node_weights[0] = 1.0f;
		uncompressed[index].node_weights[1] = 0.0f;
		if (!area_bounds_set)
		{
			area_min_x = area_max_x = uncompressed[index].position.x;
			area_min_y = area_max_y = uncompressed[index].position.y;
			area_min_z = area_max_z = uncompressed[index].position.z;
			area_bounds_set = 1;
		}
		else
		{
			if (uncompressed[index].position.x < area_min_x)
				area_min_x = uncompressed[index].position.x;
			if (uncompressed[index].position.y < area_min_y)
				area_min_y = uncompressed[index].position.y;
			if (uncompressed[index].position.z < area_min_z)
				area_min_z = uncompressed[index].position.z;
			if (uncompressed[index].position.x > area_max_x)
				area_max_x = uncompressed[index].position.x;
			if (uncompressed[index].position.y > area_max_y)
				area_max_y = uncompressed[index].position.y;
			if (uncompressed[index].position.z > area_max_z)
				area_max_z = uncompressed[index].position.z;
		}
		slot->centroid.x += uncompressed[index].position.x;
		slot->centroid.y += uncompressed[index].position.y;
		slot->centroid.z += uncompressed[index].position.z;
	}
	for (index = 0; index < triangle_count + 2; index++)
	{
		if (indices[index] >= (unsigned short)vertex_count)
			goto done;
	}
	slot->centroid.x /= (real)vertex_count;
	slot->centroid.y /= (real)vertex_count;
	slot->centroid.z /= (real)vertex_count;
	for (index = 0; index < vertex_count; index++)
	{
		real dx = uncompressed[index].position.x - slot->centroid.x;
		real dy = uncompressed[index].position.y - slot->centroid.y;
		real dz = uncompressed[index].position.z - slot->centroid.z;
		real span = dx * dx + dy * dy + dz * dz;

		if (span > radius_squared)
			radius_squared = span;
	}
	/* The transparent sort drops a mesh whose radius is under a world unit.
	These volumes are the size of the respawn zone, so the radius is the
	real extent. */
	slot->radius = sqrtf(radius_squared);
	if (slot->radius < 1.0f)
		slot->radius = 1.0f;
	rasterizer_geometry_compress_vertices(
		_rasterizer_vertex_type_model_uncompressed,
		vertex_count,
		compressed,
		vertex_count * (long)sizeof(*compressed),
		uncompressed,
		vertex_count * (long)sizeof(*uncompressed));
	if (!rasterizer_vertex_buffer_new(&slot->vertices, _rasterizer_vertex_type_model_compressed,
			vertex_count, compressed, vertex_count * (long)sizeof(*compressed)) ||
		!rasterizer_triangle_buffer_new(&slot->triangles, _triangle_buffer_type_precompiled_strip,
			triangle_count, indices))
		goto done;
	slot->triangle_count = triangle_count;
	area_part_count++;
	loaded = 1;

done:
	area_free(uncompressed);
	area_free(compressed);
	area_free(indices);
	return loaded;
}

/* 1 when at least one randoms placement was recorded, 0 when the map has
none, -1 when a placement was there and its model could not be followed. */
static int area_collect(unsigned char const *tag, unsigned long tag_size,
	unsigned long scenario_offset, char *model_name, unsigned long model_name_size)
{
	long scenery_count;
	long palette_count;
	long index;
	unsigned long scenery_offset;
	unsigned long palette_offset;
	int palette_index = -1;
	int broken = 0;

	scenery_count = area_s32(tag + scenario_offset + 0x210);
	palette_count = area_s32(tag + scenario_offset + 0x21C);
	if (scenery_count < 1 || scenery_count > 8192 || palette_count < 1 || palette_count > 512 ||
		!area_tag_place(area_u32(tag + scenario_offset + 0x214), tag_size, &scenery_offset) ||
		!area_tag_place(area_u32(tag + scenario_offset + 0x220), tag_size, &palette_offset) ||
		!area_span(scenery_offset, (unsigned long)scenery_count * 72UL, tag_size) ||
		!area_span(palette_offset, (unsigned long)palette_count * 48UL, tag_size))
		return 0;
	model_name[0] = 0;
	for (index = 0; index < scenery_count; index++)
	{
		unsigned char const *datum = tag + scenery_offset + (unsigned long)index * 72UL;
		short palette = area_s16(datum);
		char path[256];
		unsigned long scenery_address;
		unsigned long scenery_placed;

		if (palette < 0 || palette >= palette_count)
			continue;
		if (!area_copy_name(tag, tag_size,
				area_u32(tag + palette_offset + (unsigned long)palette * 48UL + 4),
				path, sizeof(path)) || !area_has_randoms(path))
			continue;
		if (palette_index < 0)
		{
			palette_index = palette;
			if (!area_find(tag, tag_size, "necs", path, &scenery_address) ||
				!area_tag_place(scenery_address, tag_size, &scenery_placed) ||
				!area_span(scenery_placed, 0x40, tag_size) ||
				!area_copy_name(tag, tag_size, area_u32(tag + scenery_placed + 0x2C),
					model_name, model_name_size))
				broken = 1;
		}
		if (broken || palette != palette_index || area_place_count >= AREA_PLACES_MAXIMUM)
			continue;
		area_places[area_place_count].position.x = area_f32(datum + 8);
		area_places[area_place_count].position.y = area_f32(datum + 12);
		area_places[area_place_count].position.z = area_f32(datum + 16);
		area_places[area_place_count].yaw = area_f32(datum + 0x14);
		area_places[area_place_count].pitch = area_f32(datum + 0x18);
		area_places[area_place_count].roll = area_f32(datum + 0x1C);
		area_place_count++;
	}
	if (broken)
		return -1;
	return area_place_count > 0 ? 1 : 0;
}

static void area_prepare(void)
{
	FILE *file = NULL;
	unsigned char *tag = NULL;
	char leaf[64];
	char path[96];
	char model_name[256];
	unsigned char header[0x18];
	unsigned long file_size;
	unsigned long tag_offset;
	unsigned long tag_size;
	unsigned long scenario_address;
	unsigned long scenario_offset;
	unsigned long model_address;
	unsigned long model_offset;
	unsigned long vertex_file;
	unsigned long index_relative;
	unsigned long part_offset;
	long part_count;
	long part_index;
	long geometry_count;
	long geometry_index;
	long best_vertices;
	long told;
	int collected;
	int best_count;
	unsigned long best_address;

	if (!area_leaf(leaf, sizeof(leaf)) ||
		snprintf(path, sizeof(path), "d:\\competitive_maps\\%s.map", leaf) >= (int)sizeof(path))
		goto fail;
	file = fopen(path, "rb");
	if (!file)
		goto fail;
	if (fseek(file, 0, SEEK_END) != 0 || (told = ftell(file)) < 0x800)
		goto fail;
	file_size = (unsigned long)told;
	if (fseek(file, 0, SEEK_SET) != 0 || fread(header, 1, sizeof(header), file) != sizeof(header) ||
		header[0] != 'd' || header[1] != 'a' || header[2] != 'e' || header[3] != 'h' ||
		area_s32(header + 4) != 7)
		goto fail;
	tag_offset = area_u32(header + 0x10);
	tag_size = area_u32(header + 0x14);
	if (tag_offset < 0x800 || tag_size < 0x28 || tag_size > AREA_TAG_MAXIMUM ||
		!area_span(tag_offset, tag_size, file_size))
		goto fail;
	tag = (unsigned char *)malloc(tag_size);
	if (!tag || fseek(file, (long)tag_offset, SEEK_SET) != 0 ||
		fread(tag, 1, tag_size, file) != tag_size)
		goto fail;
	if (!area_find(tag, tag_size, "rncs", NULL, &scenario_address) ||
		!area_tag_place(scenario_address, tag_size, &scenario_offset) ||
		!area_span(scenario_offset, 0x228, tag_size))
		goto fail;
	collected = area_collect(tag, tag_size, scenario_offset, model_name, sizeof(model_name));
	if (collected == 0)
		goto fail;
	if (collected < 0 ||
		!area_find(tag, tag_size, "2dom", model_name, &model_address) ||
		!area_tag_place(model_address, tag_size, &model_offset) ||
		!area_span(model_offset, 0xE8, tag_size))
		goto missing;
	/* The node pivot is kept and applied only if it lands the mesh on the
	spawns. Several of these volumes were exported already in world space,
	and adding the pivot there moves them off the respawn area. */
	area_node_translation.x = area_node_translation.y = area_node_translation.z = 0.0f;
	{
		long node_count = area_s32(tag + model_offset + 0xB8);
		unsigned long node_offset;

		if (node_count >= 1 &&
			area_tag_place(area_u32(tag + model_offset + 0xBC), tag_size, &node_offset) &&
			area_span(node_offset, 52, tag_size))
		{
			area_node_translation.x = area_f32(tag + node_offset + 40);
			area_node_translation.y = area_f32(tag + node_offset + 44);
			area_node_translation.z = area_f32(tag + node_offset + 48);
		}
	}
	vertex_file = area_u32(tag + 0x14);
	index_relative = area_u32(tag + 0x1C);
	geometry_count = area_s32(tag + model_offset + 0xD0);
	best_vertices = 0;
	best_count = 0;
	best_address = 0;
	if (geometry_count < 1 || geometry_count > 8 ||
		!area_tag_place(area_u32(tag + model_offset + 0xD4), tag_size, &part_offset) ||
		!area_span(part_offset, (unsigned long)geometry_count * 0x30UL, tag_size))
		goto missing;
	for (geometry_index = 0; geometry_index < geometry_count; geometry_index++)
	{
		unsigned char const *geometry = tag + part_offset + (unsigned long)geometry_index * 0x30UL;
		long count = area_s32(geometry + 0x24);
		unsigned long address = area_u32(geometry + 0x28);
		unsigned long offset;
		long total = 0;
		long candidate;

		if (count < 1 || count > 32 || !area_tag_place(address, tag_size, &offset) ||
			!area_span(offset, (unsigned long)count * 0x84UL, tag_size))
			continue;
		for (candidate = 0; candidate < count; candidate++)
		{
			long verts;
			long tris;

			if (area_part_fits(tag + offset + (unsigned long)candidate * 0x84UL, &verts, &tris))
				total += verts;
		}
		if (total > best_vertices)
		{
			best_vertices = total;
			best_count = count;
			best_address = address;
		}
	}
	if (best_vertices < 3 || !area_tag_place(best_address, tag_size, &part_offset))
		goto missing;
	part_count = best_count;
	for (part_index = 0; part_index < part_count && area_part_count < AREA_PARTS_MAXIMUM; part_index++)
	{
		if (!area_load_part(file, file_size, vertex_file, index_relative,
				tag + part_offset + (unsigned long)part_index * 0x84UL))
			goto missing;
	}
	if (area_part_count < 1)
		goto missing;
	area_free(tag);
	fclose(file);
	area_state = AREA_STATE_LOADED;
	return;

missing:
	if (!area_logged)
	{
		area_logged = 1;
		platform_log("competitive: random areas were not loaded from competitive_maps/%s.map", leaf);
	}
fail:
	if (file)
		fclose(file);
	area_free(tag);
	area_discard();
}

static int area_count_inside(real ox, real oy, real oz,
	struct player_starting_location const *starts, short count)
{
	short index;
	int inside = 0;

	if (!area_bounds_set || !starts || count < 1)
		return 0;
	for (index = 0; index < count; index++)
	{
		real x = starts[index].position.x;
		real y = starts[index].position.y;
		real z = starts[index].position.z;

		if (x < area_min_x + ox - 1.5f || x > area_max_x + ox + 1.5f ||
			y < area_min_y + oy - 1.5f || y > area_max_y + oy + 1.5f ||
			z < area_min_z + oz - 2.0f || z > area_max_z + oz + 2.0f)
			continue;
		inside++;
	}
	return inside;
}

/* The mesh is kept when this offset puts it on the starts. A speck that
covers one start is not a respawn area. */
static int area_place_on_starts(struct player_starting_location const *starts, short count)
{
	real ox[4];
	real oy[4];
	real oz[4];
	int scores[4];
	int best = 0;
	int index;
	int need;

	ox[0] = oy[0] = oz[0] = 0.0f;
	ox[1] = area_place_count > 0 ? area_places[0].position.x : 0.0f;
	oy[1] = area_place_count > 0 ? area_places[0].position.y : 0.0f;
	oz[1] = area_place_count > 0 ? area_places[0].position.z : 0.0f;
	ox[2] = area_node_translation.x;
	oy[2] = area_node_translation.y;
	oz[2] = area_node_translation.z;
	ox[3] = ox[1] + ox[2];
	oy[3] = oy[1] + oy[2];
	oz[3] = oz[1] + oz[2];
	for (index = 0; index < 4; index++)
		scores[index] = area_count_inside(ox[index], oy[index], oz[index], starts, count);
	for (index = 1; index < 4; index++)
	{
		if (scores[index] > scores[best])
			best = index;
	}
	need = count < 4 ? (int)count : 4;
	if (need < 1 || scores[best] < need)
		return 0;
	area_offset.x = ox[best];
	area_offset.y = oy[best];
	area_offset.z = oz[best];
	return 1;
}

static void area_box_vertex(struct model_vertex_uncompressed *vertex, real x, real y, real z)
{
	memset(vertex, 0, sizeof(*vertex));
	vertex->position.x = x;
	vertex->position.y = y;
	vertex->position.z = z;
	vertex->normal.k = 1.0f;
	vertex->node_weights[0] = 1.0f;
}

struct area_outline_point
{
	real x;
	real y;
	real z;
};

static real area_outline_cross(struct area_outline_point const *origin,
	struct area_outline_point const *a, struct area_outline_point const *b)
{
	return (a->x - origin->x) * (b->y - origin->y) - (a->y - origin->y) * (b->x - origin->x);
}

static void area_outline_sort(struct area_outline_point *points, int count)
{
	int index;
	int earlier;

	for (index = 1; index < count; index++)
	{
		struct area_outline_point key = points[index];

		earlier = index - 1;
		while (earlier >= 0 && (points[earlier].x > key.x ||
			(points[earlier].x == key.x && points[earlier].y > key.y)))
		{
			points[earlier + 1] = points[earlier];
			earlier--;
		}
		points[earlier + 1] = key;
	}
}

/* Andrew's chain. The result is counterclockwise, and only the outer
points remain. */
static int area_outline_hull(struct area_outline_point *points, int count,
	struct area_outline_point *hull)
{
	int index;
	int lower;
	int upper;
	int kept;

	if (count < 1)
		return 0;
	area_outline_sort(points, count);
	kept = 1;
	for (index = 1; index < count; index++)
	{
		real dx = points[index].x - points[kept - 1].x;
		real dy = points[index].y - points[kept - 1].y;

		if (dx * dx + dy * dy > 0.0001f)
			points[kept++] = points[index];
	}
	count = kept;
	if (count <= 2)
	{
		for (index = 0; index < count; index++)
			hull[index] = points[index];
		return count;
	}
	lower = 0;
	for (index = 0; index < count; index++)
	{
		while (lower >= 2 && area_outline_cross(&hull[lower - 2], &hull[lower - 1],
			&points[index]) <= 0.0f)
			lower--;
		hull[lower++] = points[index];
	}
	upper = lower + 1;
	for (index = count - 2; index >= 0; index--)
	{
		while (lower >= upper && area_outline_cross(&hull[lower - 2], &hull[lower - 1],
			&points[index]) <= 0.0f)
			lower--;
		hull[lower++] = points[index];
	}
	return lower - 1;
}

/* Push each outer point out along the corner so the edge clears the
starts by one margin. A sharp corner is kept from throwing a long spike. */
static void area_outline_expand(struct area_outline_point const *hull, int count,
	struct area_outline_point *expanded)
{
	int index;

	for (index = 0; index < count; index++)
	{
		struct area_outline_point const *previous = &hull[(index + count - 1) % count];
		struct area_outline_point const *current = &hull[index];
		struct area_outline_point const *next = &hull[(index + 1) % count];
		real ex = current->x - previous->x;
		real ey = current->y - previous->y;
		real fx = next->x - current->x;
		real fy = next->y - current->y;
		real elen = sqrtf(ex * ex + ey * ey);
		real flen = sqrtf(fx * fx + fy * fy);
		real nx;
		real ny;
		real mx;
		real my;
		real mlen;
		real dot;
		real scale;

		expanded[index] = *current;
		if (elen < 0.0001f || flen < 0.0001f)
			continue;
		nx = ey / elen;
		ny = -ex / elen;
		mx = nx + fy / flen;
		my = ny + -fx / flen;
		mlen = sqrtf(mx * mx + my * my);
		if (mlen < 0.0001f)
		{
			mx = nx;
			my = ny;
		}
		else
		{
			mx /= mlen;
			my /= mlen;
		}
		dot = mx * nx + my * ny;
		if (dot < 0.5f)
			dot = 0.5f;
		scale = AREA_OUTLINE_MARGIN / dot;
		expanded[index].x += mx * scale;
		expanded[index].y += my * scale;
	}
}

static int area_outline_emit(struct model_vertex_uncompressed *uncompressed,
	unsigned short *indices, long *vertex_count, long *triangle_count,
	struct area_outline_point const *poly, int count)
{
	int index;
	long base;

	if (count < 3 || *vertex_count + count > AREA_OUTLINE_VERTICES ||
		*triangle_count + (count - 2) > AREA_OUTLINE_TRIANGLES)
		return 0;
	base = *vertex_count;
	for (index = 0; index < count; index++)
		area_box_vertex(&uncompressed[base + index], poly[index].x, poly[index].y,
			poly[index].z + AREA_SURFACE_LIFT);
	for (index = 1; index + 1 < count; index++)
	{
		long tri = *triangle_count;

		indices[tri * 3] = (unsigned short)base;
		indices[tri * 3 + 1] = (unsigned short)(base + index);
		indices[tri * 3 + 2] = (unsigned short)(base + index + 1);
		(*triangle_count)++;
	}
	*vertex_count += count;
	return 1;
}

static int area_outline_find(short *parent, int index)
{
	while (parent[index] != index)
	{
		parent[index] = parent[parent[index]];
		index = parent[index];
	}
	return index;
}

static int area_generate(struct player_starting_location const *starts, short count)
{
	struct model_vertex_uncompressed *uncompressed = NULL;
	struct model_vertex_compressed *compressed = NULL;
	unsigned short *indices = NULL;
	struct area_outline_point *points = NULL;
	struct area_outline_point *group = NULL;
	struct area_outline_point *hull = NULL;
	struct area_outline_point *expanded = NULL;
	short *parent = NULL;
	struct area_part *slot;
	short index;
	short other;
	int limit;
	int root;
	long vertex_count = 0;
	long triangle_count = 0;
	long corner;
	real radius_squared = 0.0f;
	int loaded = 0;

	if (!starts || count < 1 || area_part_count != 0)
		return 0;
	limit = count > AREA_OUTLINE_POINTS ? AREA_OUTLINE_POINTS : (int)count;
	uncompressed = (struct model_vertex_uncompressed *)malloc(
		(unsigned long)AREA_OUTLINE_VERTICES * sizeof(*uncompressed));
	compressed = (struct model_vertex_compressed *)malloc(
		(unsigned long)AREA_OUTLINE_VERTICES * sizeof(*compressed));
	indices = (unsigned short *)malloc(
		(unsigned long)AREA_OUTLINE_TRIANGLES * 3UL * sizeof(*indices));
	points = (struct area_outline_point *)malloc((unsigned long)limit * sizeof(*points));
	group = (struct area_outline_point *)malloc((unsigned long)limit * sizeof(*group));
	hull = (struct area_outline_point *)malloc((unsigned long)limit * 2UL * sizeof(*hull));
	expanded = (struct area_outline_point *)malloc((unsigned long)limit * sizeof(*expanded));
	parent = (short *)malloc((unsigned long)limit * sizeof(*parent));
	if (!uncompressed || !compressed || !indices || !points || !group || !hull ||
		!expanded || !parent)
		goto done;
	for (index = 0; index < limit; index++)
	{
		points[index].x = starts[index].position.x;
		points[index].y = starts[index].position.y;
		points[index].z = starts[index].position.z;
		parent[index] = index;
	}
	for (index = 0; index < limit; index++)
	{
		for (other = 0; other < index; other++)
		{
			real dx = points[index].x - points[other].x;
			real dy = points[index].y - points[other].y;
			real dz = points[index].z - points[other].z;

			if (dx * dx + dy * dy <= AREA_OUTLINE_LINK * AREA_OUTLINE_LINK &&
				dz <= AREA_OUTLINE_LINK_Z && dz >= -AREA_OUTLINE_LINK_Z)
				parent[area_outline_find(parent, index)] = area_outline_find(parent, other);
		}
	}
	for (root = 0; root < limit; root++)
	{
		int members = 0;
		int hull_count;
		struct area_outline_point quad[4];

		if (area_outline_find(parent, root) != root)
			continue;
		for (index = 0; index < limit; index++)
		{
			if (area_outline_find(parent, index) != root)
				continue;
			group[members++] = points[index];
		}
		hull_count = area_outline_hull(group, members, hull);
		if (hull_count >= 3)
		{
			area_outline_expand(hull, hull_count, expanded);
			area_outline_emit(uncompressed, indices, &vertex_count, &triangle_count,
				expanded, hull_count);
		}
		else if (hull_count == 2)
		{
			real dx = hull[1].x - hull[0].x;
			real dy = hull[1].y - hull[0].y;
			real length = sqrtf(dx * dx + dy * dy);
			real nx;
			real ny;
			real ex;
			real ey;

			if (length < 0.0001f)
				hull_count = 1;
			else
			{
				nx = dy / length * AREA_OUTLINE_MARGIN;
				ny = -dx / length * AREA_OUTLINE_MARGIN;
				ex = dx / length * AREA_OUTLINE_MARGIN;
				ey = dy / length * AREA_OUTLINE_MARGIN;
				quad[0].x = hull[0].x - ex - nx;
				quad[0].y = hull[0].y - ey - ny;
				quad[0].z = hull[0].z;
				quad[1].x = hull[1].x + ex - nx;
				quad[1].y = hull[1].y + ey - ny;
				quad[1].z = hull[1].z;
				quad[2].x = hull[1].x + ex + nx;
				quad[2].y = hull[1].y + ey + ny;
				quad[2].z = hull[1].z;
				quad[3].x = hull[0].x - ex + nx;
				quad[3].y = hull[0].y - ey + ny;
				quad[3].z = hull[0].z;
				area_outline_emit(uncompressed, indices, &vertex_count, &triangle_count, quad, 4);
			}
		}
		if (hull_count == 1)
		{
			real margin = AREA_OUTLINE_MARGIN;

			quad[0].x = hull[0].x - margin;
			quad[0].y = hull[0].y - margin;
			quad[0].z = hull[0].z;
			quad[1].x = hull[0].x + margin;
			quad[1].y = hull[0].y - margin;
			quad[1].z = hull[0].z;
			quad[2].x = hull[0].x + margin;
			quad[2].y = hull[0].y + margin;
			quad[2].z = hull[0].z;
			quad[3].x = hull[0].x - margin;
			quad[3].y = hull[0].y + margin;
			quad[3].z = hull[0].z;
			area_outline_emit(uncompressed, indices, &vertex_count, &triangle_count, quad, 4);
		}
	}
	if (vertex_count < 3 || triangle_count < 1)
		goto done;
	slot = &area_parts[0];
	slot->centroid.x = slot->centroid.y = slot->centroid.z = 0.0f;
	for (corner = 0; corner < vertex_count; corner++)
	{
		slot->centroid.x += uncompressed[corner].position.x;
		slot->centroid.y += uncompressed[corner].position.y;
		slot->centroid.z += uncompressed[corner].position.z;
	}
	slot->centroid.x /= (real)vertex_count;
	slot->centroid.y /= (real)vertex_count;
	slot->centroid.z /= (real)vertex_count;
	for (corner = 0; corner < vertex_count; corner++)
	{
		real dx = uncompressed[corner].position.x - slot->centroid.x;
		real dy = uncompressed[corner].position.y - slot->centroid.y;
		real dz = uncompressed[corner].position.z - slot->centroid.z;
		real span = dx * dx + dy * dy + dz * dz;

		if (span > radius_squared)
			radius_squared = span;
	}
	slot->radius = sqrtf(radius_squared);
	if (slot->radius < 1.0f)
		slot->radius = 1.0f;
	rasterizer_geometry_compress_vertices(
		_rasterizer_vertex_type_model_uncompressed,
		vertex_count,
		compressed,
		vertex_count * (long)sizeof(*compressed),
		uncompressed,
		vertex_count * (long)sizeof(*uncompressed));
	if (!rasterizer_vertex_buffer_new(&slot->vertices, _rasterizer_vertex_type_model_compressed,
			vertex_count, compressed, vertex_count * (long)sizeof(*compressed)) ||
		!rasterizer_triangle_buffer_new(&slot->triangles, _triangle_buffer_type_triangles,
			triangle_count, indices))
		goto done;
	slot->triangle_count = triangle_count;
	area_part_count = 1;
	area_offset.x = area_offset.y = area_offset.z = 0.0f;
	loaded = 1;

done:
	area_free(uncompressed);
	area_free(compressed);
	area_free(indices);
	area_free(points);
	area_free(group);
	area_free(hull);
	area_free(expanded);
	area_free(parent);
	return loaded;
}

static void area_draw(struct area_part const *part, long identifier)
{
	real_matrix4x3 matrix;
	real_point3d centroid;
	struct rasterizer_model_begin_parameters parameters;
	static real_rgb_color colors[4];
	static real values[4];
	short color_index;

	for (color_index = 0; color_index < 4; color_index++)
	{
		colors[color_index].red = colors[color_index].green = colors[color_index].blue = 1.0f;
		values[color_index] = 0.0f;
	}
	matrix4x3_rotation_from_angles(&matrix, 0.0f, 0.0f, 0.0f);
	matrix.position = area_offset;
	centroid.x = part->centroid.x + area_offset.x;
	centroid.y = part->centroid.y + area_offset.y;
	centroid.z = part->centroid.z + area_offset.z;
	memset(&parameters, 0, sizeof(parameters));
	parameters.unique_identifier = (unsigned long)identifier;
	parameters.skinning.node_matrices = &matrix;
	parameters.skinning.node_matrix_count = 1;
	parameters.lighting.ambient_color = *global_real_rgb_white;
	parameters.lighting.reflection_tint_color = *global_real_argb_white;
	parameters.lighting.shadow_vector.j = 1.0f;
	parameters.lighting.shadow_color = *global_real_rgb_black;
	parameters.animation.colors = colors;
	parameters.animation.values = values;
	parameters.centroid = centroid;
	parameters.radius = part->radius;
	parameters.base_map_scale.i = 1.0f;
	parameters.base_map_scale.j = 1.0f;
	rasterizer_profile_enable(FALSE);
	rasterizer_models_begin(FALSE);
	rasterizer_model_begin(&parameters, TRUE);
	rasterizer_model_transparent_geometry_submit(
		(struct shader *)area_shader,
		0,
		&part->triangles,
		NONE,
		part->triangle_count,
		&part->vertices,
		NONE,
		&centroid,
		NULL);
	rasterizer_model_end();
	rasterizer_models_end();
	rasterizer_profile_enable(TRUE);
}

void performance_random_area_reset(void)
{
	area_discard();
	area_logged = 0;
	area_authored_tried = 0;
	area_state = AREA_STATE_NONE;
}

void performance_random_area_render(struct player_starting_location const *starts, short count)
{
	int part_index;

	if (area_state == AREA_STATE_NONE && !area_authored_tried)
	{
		area_authored_tried = 1;
		area_prepare();
	}
	if (area_state == AREA_STATE_LOADED)
	{
		if (!starts || count < 1)
			return;
		if (!area_place_on_starts(starts, count) || !area_bitmap_make())
		{
			area_discard();
			area_state = AREA_STATE_NONE;
		}
		else
			area_state = AREA_STATE_READY;
	}
	if (area_state == AREA_STATE_NONE)
	{
		/* No placed volume covers these starts. Each group of starts gets
		the outline of its outer points. */
		if (!starts || count < 1)
			return;
		if (!area_generate(starts, count) || !area_bitmap_make())
		{
			area_discard();
			area_state = AREA_STATE_FAILED;
			return;
		}
		area_state = AREA_STATE_READY;
	}
	if (area_state != AREA_STATE_READY)
		return;
	for (part_index = 0; part_index < area_part_count; part_index++)
		area_draw(&area_parts[part_index], part_index + 1);
}

boolean performance_random_area_bitmap(struct bitmap_data *bitmap, void *buffer)
{
	if (bitmap != &area_bitmap)
		return FALSE;
	if (!buffer || !area_pixels || area_pixel_bytes < 1 ||
		bitmap->pixels_size != area_pixel_bytes)
	{
		if (buffer && bitmap->pixels_size > 0 && bitmap->pixels_size <= 1024 * 1024)
			memset(buffer, 0, (size_t)bitmap->pixels_size);
		return TRUE;
	}
	memcpy(buffer, area_pixels, (size_t)area_pixel_bytes);
	return TRUE;
}
