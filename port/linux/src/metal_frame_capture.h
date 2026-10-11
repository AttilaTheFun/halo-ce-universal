/* Included by d3d8_gl.c after its private device/state helpers.
 * Deliberately default-disabled: only the existing gpu_trace_frame selects a
 * diagnostic capture. All wire numbers are Xbox uint32 or explicit floats.
 * The final SHA256 manifest is written by tools/metal_frame_capture.py.
 */
#if defined(HALO_MACOS) && HALO_MACOS_METAL_FRAME_CAPTURE
#if !HALO_MACOS_METAL_SHADER_CAPTURE
#error "Full draw capture requires the paired shader corpus"
#endif
extern unsigned char rasterizer_globals[];

struct metal_frame_target
{
	struct metal_frame_target *next;
	struct render_target_entry *entry;
	unsigned long version;
	BOOL captured;
};

struct metal_frame_query
{
	struct metal_frame_query *next;
	GLuint query;
	unsigned long slot, event;
};

static struct
{
	BOOL started, finished, failed;
	char directory[1024], scratch[1024];
	FILE *events;
	unsigned long event_count, draw_count, clear_count, copy_count, bind_count;
	struct metal_frame_target *targets;
	struct metal_frame_query *queries;
} metal_frame;

static BOOL metal_frame_file(const char *name, const void *data, unsigned long bytes)
{
	char path[1280]; FILE *file; BOOL ok;
	if (snprintf(path, sizeof(path), "%s/%s", metal_frame.directory, name) >= (int)sizeof(path)) return FALSE;
	file = fopen(path, "wb"); if (!file) return FALSE;
	ok = fwrite(data, 1, bytes, file) == bytes;
	if (fclose(file) != 0) ok = FALSE;
	return ok;
}

/* FNV chooses a candidate filename only; matching bytes are checked before
 * reuse. The Python freezer independently hashes every payload with SHA256. */
static void metal_frame_blob(FILE *file, const void *data, unsigned long bytes)
{
	const unsigned char *p = data;
	unsigned long long hash = 14695981039346656037ULL;
	unsigned long i; char name[128], path[1280]; FILE *old;
	unsigned char buffer[4096]; BOOL equal = TRUE;
	if (!data && bytes) { metal_frame.failed = TRUE; fputs("null", file); return; }
	for (i = 0; i < bytes; i++) { hash ^= p[i]; hash *= 1099511628211ULL; }
	snprintf(name, sizeof(name), "blobs/%08lx%08lx-%lu.bin", (unsigned long)(hash >> 32), (unsigned long)(hash & 0xffffffffULL), bytes);
	snprintf(path, sizeof(path), "%s/%s", metal_frame.directory, name);
	old = fopen(path, "rb");
	if (old)
	{
		for (i = 0; i < bytes;)
		{
			unsigned long count = bytes - i < sizeof(buffer) ? bytes - i : sizeof(buffer);
			if (fread(buffer, 1, count, old) != count || memcmp(buffer, p + i, count)) { equal = FALSE; break; }
			i += count;
		}
		if (equal && fgetc(old) != EOF) equal = FALSE;
		fclose(old);
		if (!equal) { metal_frame.failed = TRUE; fputs("null", file); return; }
	}
	else if (!metal_frame_file(name, data, bytes)) { metal_frame.failed = TRUE; fputs("null", file); return; }
	fprintf(file, "{\"file\":\"%s\",\"bytes\":%lu}", name, bytes);
}

static void metal_frame_disk_blob(FILE *file, const char *name, unsigned long bytes)
{
	char path[1280]; unsigned char *data; FILE *source;
	snprintf(path, sizeof(path), "%s/%s", metal_frame.scratch, name);
	source = fopen(path, "rb"); data = malloc(bytes ? bytes : 1);
	if (!source || !data) { if (source) fclose(source); free(data); metal_frame.failed = TRUE; fputs("null", file); return; }
	if (fread(data, 1, bytes, source) != bytes || fgetc(source) != EOF) { metal_frame.failed = TRUE; fputs("null", file); }
	else metal_frame_blob(file, data, bytes);
	fclose(source); free(data);
}

static BOOL metal_frame_start(void)
{
	unsigned long suffix; char parent[1280], path[1280];
	if (!trace_frame()) return FALSE;
	if (metal_frame.started) return !metal_frame.finished && metal_frame.events != NULL;
	metal_frame.started = TRUE;
	snprintf(parent, sizeof(parent), "%s/metal-frames", platform_save_root());
	if (mkdir(parent, 0700) != 0 && errno != EEXIST) goto failure;
	for (suffix = 0; suffix < 10000; suffix++)
	{
		if (suffix) snprintf(metal_frame.directory, sizeof(metal_frame.directory), "%s/frame%lu-%lu", parent, device.frame, suffix);
		else snprintf(metal_frame.directory, sizeof(metal_frame.directory), "%s/frame%lu", parent, device.frame);
		if (mkdir(metal_frame.directory, 0700) == 0) break;
		if (errno != EEXIST) goto failure;
	}
	if (suffix == 10000) goto failure;
	snprintf(path, sizeof(path), "%s/blobs", metal_frame.directory); if (mkdir(path, 0700)) goto failure;
	snprintf(metal_frame.scratch, sizeof(metal_frame.scratch), "%s/readback", metal_frame.directory); if (mkdir(metal_frame.scratch, 0700)) goto failure;
	snprintf(path, sizeof(path), "%s/events.jsonl", metal_frame.directory);
	metal_frame.events = fopen(path, "w"); if (!metal_frame.events) goto failure;
	platform_log("METAL_FRAME_CAPTURE begin %lu %s", device.frame, metal_frame.directory);
	return TRUE;
failure:
	metal_frame.failed = TRUE; return FALSE;
}

static FILE *metal_frame_record(const char *kind, unsigned long id)
{
	char path[1280]; FILE *file;
	snprintf(path, sizeof(path), "%s/%s-%04lu.json", metal_frame.directory, kind, id);
	file = fopen(path, "w"); if (!file) metal_frame.failed = TRUE; return file;
}

static void metal_frame_event(const char *kind)
{
	fprintf(metal_frame.events, "{\"schema_version\":1,\"event\":%lu,\"kind\":\"%s\",\"frame\":%lu", metal_frame.event_count++, kind, device.frame);
}

static void metal_frame_event_end(void)
{
	fputs("}\n", metal_frame.events);
	if (ferror(metal_frame.events)) metal_frame.failed = TRUE;
}

static struct metal_frame_target *metal_frame_target(struct render_target_entry *entry)
{
	struct metal_frame_target *target;
	if (!entry) return NULL;
	for (target = metal_frame.targets; target; target = target->next) if (target->entry == entry) return target;
	target = calloc(1, sizeof(*target));
	if (!target) { metal_frame.failed = TRUE; return NULL; }
	target->entry = entry; target->next = metal_frame.targets; metal_frame.targets = target;
	return target;
}

static void metal_frame_target_ref(FILE *file, struct metal_frame_target *target)
{
	if (!target) fputs("null", file);
	else fprintf(file, "{\"target_id\":%u,\"version\":%lu}", target->entry->target.texture, target->version);
}

/* Private diagnostic GPU readback; see its independent control validation. */
#include "metal_frame_readback.h"

static void metal_frame_target_snapshot(struct metal_frame_target *target, BOOL mutation)
{
	struct xgpu_render_target *t; FILE *file;
	GLint read_fbo, draw_fbo; unsigned char *pixels = NULL; BOOL ok = TRUE;
	if (!target || (target->captured && !mutation)) return;
	/* Never include diagnostic GPU draws in an original occlusion query. */
	if (device.visibility_test_active) { metal_frame.failed = TRUE; return; }
	t = &target->entry->target;
	if (target->captured) target->version++;
	file = metal_frame_record("target", (unsigned long)t->texture * 10000 + target->version);
	if (!file) return;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	fprintf(file, "{\"schema_version\":1,\"kind\":\"target_version\",\"target_id\":%u,\"version\":%lu,\"source_data\":%lu,\"source_width\":%lu,\"source_height\":%lu,\"width\":%lu,\"height\":%lu,\"depth\":%s,\"orientation\":\"top_left\",", t->texture, target->version, t->data, t->width, t->height, t->gl_width, t->gl_height, t->depth ? "true" : "false");
	if (t->depth)
	{
		ok = metal_frame_gpu_readback(metal_frame.scratch, "snapshot", t->gl_width, t->gl_height, t->texture);
		fputs("\"actual_metal_format\":\"depth32float_stencil8\",\"declared_gl_format\":\"depth24_stencil8\",\"depth_values\":", file);
		if (ok) metal_frame_disk_blob(file, "snapshot-depth.float32", t->gl_width * t->gl_height * 4); else fputs("null", file);
		fputs(",\"stencil_values\":", file);
		if (ok) metal_frame_disk_blob(file, "snapshot-stencil.uint8", t->gl_width * t->gl_height); else fputs("null", file);
		fputs(",\"stencil_control\":", file);
		if (ok) metal_frame_disk_blob(file, "snapshot-control-stencil.uint8", t->gl_width * t->gl_height); else fputs("null", file);
	}
	else
	{
		pixels = malloc(t->gl_width * t->gl_height * 4);
		if (!pixels) ok = FALSE;
		else
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_get(t->texture, 0));
			glReadPixels(0, 0, t->gl_width, t->gl_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			if (glGetError() != GL_NO_ERROR) ok = FALSE;
		}
		fputs("\"color_format\":\"rgba8unorm\",\"color\":", file);
		if (ok) metal_frame_blob(file, pixels, t->gl_width * t->gl_height * 4); else fputs("null", file);
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	free(pixels);
	if (!ok) metal_frame.failed = TRUE;
	fprintf(file, ",\"complete\":%s}\n", ok && !metal_frame.failed ? "true" : "false");
	if (fclose(file)) metal_frame.failed = TRUE;
	target->captured = TRUE;
}

static void metal_frame_bound_refs(FILE *file, BOOL snapshot, BOOL mutation)
{
	struct metal_frame_target *color = metal_frame_target(render_target_get(device.render_target));
	struct metal_frame_target *depth = metal_frame_target(render_target_get(device.depth_stencil));
	if (snapshot) { metal_frame_target_snapshot(color, mutation); metal_frame_target_snapshot(depth, mutation); }
	fputs("{\"color\":", file); metal_frame_target_ref(file, color);
	fputs(",\"depth_stencil\":", file); metal_frame_target_ref(file, depth); fputc('}', file);
}

static void metal_frame_bind(void)
{
	if (!metal_frame_start()) return;
	metal_frame_event("target_bind");
	fprintf(metal_frame.events, ",\"color_data\":%lu,\"depth_data\":%lu", device.render_target ? device.render_target->Data : 0, device.depth_stencil ? device.depth_stencil->Data : 0);
	metal_frame.bind_count++; metal_frame_event_end();
}

static void metal_frame_state(FILE *file)
{
	DWORD *rs = D3D__RenderState; GLint scissor[4], viewport[4], enabled;
	GLint actual[16]; GLfloat depth_range[2], offset[2], blend_color[4]; unsigned long i;
	glGetIntegerv(GL_SCISSOR_TEST, &enabled); glGetIntegerv(GL_SCISSOR_BOX, scissor); glGetIntegerv(GL_VIEWPORT, viewport);
	fprintf(file, "\"viewport\":{\"origin_x\":%lu,\"origin_y\":%lu,\"width\":%lu,\"height\":%lu,\"znear\":%.9g,\"zfar\":%.9g},\"actual_gl_viewport\":[%ld,%ld,%ld,%ld],\"scissor\":{\"enabled\":%s,\"box_gl\":[%ld,%ld,%ld,%ld]},", device.viewport.X, device.viewport.Y, device.viewport.Width, device.viewport.Height, device.viewport.MinZ, device.viewport.MaxZ, (long)viewport[0], (long)viewport[1], (long)viewport[2], (long)viewport[3], enabled ? "true" : "false", (long)scissor[0], (long)scissor[1], (long)scissor[2], (long)scissor[3]);
	fprintf(file, "\"render_state_d3d\":{\"depth_enable\":%lu,\"depth_write\":%lu,\"depth_compare\":%lu,\"cull\":%lu,\"front_face\":%lu,\"color_write\":%lu,\"blend_enable\":%lu,\"blend_source\":%lu,\"blend_destination\":%lu,\"blend_op\":%lu,\"offset_enable\":%lu,\"offset_slope\":%.9g,\"offset_units\":%.9g,\"stencil_enable\":%lu,\"stencil_compare\":%lu,\"stencil_ref\":%lu,\"stencil_read_mask\":%lu,\"stencil_write_mask\":%lu,\"stencil_fail\":%lu,\"stencil_depth_fail\":%lu,\"stencil_pass\":%lu},", rs[D3DRS_ZENABLE], rs[D3DRS_ZWRITEENABLE], rs[D3DRS_ZFUNC], rs[D3DRS_CULLMODE], rs[D3DRS_FRONTFACE], rs[D3DRS_COLORWRITEENABLE], rs[D3DRS_ALPHABLENDENABLE], rs[D3DRS_SRCBLEND], rs[D3DRS_DESTBLEND], rs[D3DRS_BLENDOP], rs[D3DRS_SOLIDOFFSETENABLE], dword_to_float(rs[D3DRS_POLYGONOFFSETZSLOPESCALE]), dword_to_float(rs[D3DRS_POLYGONOFFSETZOFFSET]), rs[D3DRS_STENCILENABLE], rs[D3DRS_STENCILFUNC], rs[D3DRS_STENCILREF], rs[D3DRS_STENCILMASK], rs[D3DRS_STENCILWRITEMASK], rs[D3DRS_STENCILFAIL], rs[D3DRS_STENCILZFAIL], rs[D3DRS_STENCILPASS]);
	fputs("\"render_state_words\":", file); metal_frame_blob(file, D3D__RenderState, sizeof(D3D__RenderState));
	fputs(",\"texture_state_words\":", file); metal_frame_blob(file, D3D__TextureState, sizeof(D3D__TextureState)); fputc(',', file);
	fputs("\"viewport_bits\":", file); metal_frame_blob(file, &device.viewport, sizeof(device.viewport));
	fprintf(file, ",\"original_depth_request\":{\"auto_depth_stencil_format\":%lu,\"actual_port_surface_format_word\":%lu,\"actual_port_surface_size_word\":%lu,\"floating_point_zbuffer_byte\":%u,\"floating_point_zbuffer_offset\":60},", (unsigned long)device.presentation.AutoDepthStencilFormat, device.depth_stencil ? device.depth_stencil->Format : 0, device.depth_stencil ? device.depth_stencil->Size : 0, (unsigned int)rasterizer_globals[60]);
	glGetIntegerv(GL_DEPTH_TEST, &actual[0]); glGetIntegerv(GL_DEPTH_WRITEMASK, &actual[1]); glGetIntegerv(GL_DEPTH_FUNC, &actual[2]);
	glGetIntegerv(GL_CULL_FACE, &actual[3]); glGetIntegerv(GL_CULL_FACE_MODE, &actual[4]); glGetIntegerv(GL_FRONT_FACE, &actual[5]);
	glGetIntegerv(GL_BLEND, &actual[6]); glGetIntegerv(GL_BLEND_SRC_RGB, &actual[7]); glGetIntegerv(GL_BLEND_DST_RGB, &actual[8]); glGetIntegerv(GL_BLEND_EQUATION_RGB, &actual[9]);
	glGetIntegerv(GL_STENCIL_TEST, &actual[10]); glGetIntegerv(GL_STENCIL_FUNC, &actual[11]); glGetIntegerv(GL_STENCIL_REF, &actual[12]); glGetIntegerv(GL_STENCIL_VALUE_MASK, &actual[13]); glGetIntegerv(GL_STENCIL_WRITEMASK, &actual[14]); glGetIntegerv(GL_POLYGON_OFFSET_FILL, &actual[15]);
	hostgl_glGetFloatv(GL_DEPTH_RANGE, depth_range); hostgl_glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &offset[0]); hostgl_glGetFloatv(GL_POLYGON_OFFSET_UNITS, &offset[1]); hostgl_glGetFloatv(GL_BLEND_COLOR, blend_color);
	fputs("\"actual_gl_state\":{\"depth_cull_blend_stencil_offset_words\":[", file);
	for (i = 0; i < 16; i++) fprintf(file, "%s%ld", i ? "," : "", (long)actual[i]);
	fprintf(file, "],\"depth_range\":[%.9g,%.9g],\"polygon_offset\":[%.9g,%.9g],\"blend_color\":[%.9g,%.9g,%.9g,%.9g]},", depth_range[0], depth_range[1], offset[0], offset[1], blend_color[0], blend_color[1], blend_color[2], blend_color[3]);
}

static void metal_frame_texture(FILE *file, unsigned long stage)
{
	D3DBaseTexture *texture = device.textures[stage]; DWORD *ts = D3D__TextureState[stage];
	unsigned long mode = stage_texture_mode(stage), level; struct xgpu_texture_description d;
	struct xgpu_render_target *alias; GLint active, binding = 0; GLenum gl_target;
	if (!texture || !texture->Data || mode == 0 || mode == 4 || mode == 5 || mode == 17) { fputs("null", file); return; }
	xgpu_texture_describe(texture->Format, texture->Size, &d); alias = xgpu_render_target_find(texture->Data);
	gl_target = d.cube_map ? GL_TEXTURE_CUBE_MAP : d.depth > 1 ? GL_TEXTURE_3D : GL_TEXTURE_2D;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active); glActiveTexture(GL_TEXTURE0 + stage);
	glGetIntegerv(gl_target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_BINDING_CUBE_MAP : gl_target == GL_TEXTURE_3D ? GL_TEXTURE_BINDING_3D : GL_TEXTURE_BINDING_2D, &binding); glActiveTexture(active);
	fprintf(file, "{\"source_data\":%lu,\"format_word\":%lu,\"size_word\":%lu,\"width\":%lu,\"height\":%lu,\"depth\":%lu,\"levels\":%lu,\"actual_gl_texture\":%ld,\"sampler\":{\"min_filter\":%lu,\"mag_filter\":%lu,\"mip_filter\":%lu,\"address_u\":%lu,\"address_v\":%lu,\"address_w\":%lu,\"max_mip_level\":%lu,\"max_anisotropy\":%lu,\"mip_lod_bias\":%.9g,\"border_color\":%lu}", texture->Data, texture->Format, texture->Size, d.width, d.height, d.depth, d.levels, (long)binding, ts[D3DTSS_MINFILTER], ts[D3DTSS_MAGFILTER], ts[D3DTSS_MIPFILTER], ts[D3DTSS_ADDRESSU], ts[D3DTSS_ADDRESSV], ts[D3DTSS_ADDRESSW], ts[D3DTSS_MAXMIPLEVEL], ts[D3DTSS_MAXANISOTROPY], dword_to_float(ts[D3DTSS_MIPMAPLODBIAS]), ts[D3DTSS_BORDERCOLOR]);
	if (alias)
	{
		fputs(",\"render_target_aliases\":[", file);
		for (level = 0; level < d.levels; level++)
		{
			struct xgpu_render_target *at = xgpu_render_target_find(texture->Data + xgpu_texture_level_offset(&d, level));
			struct render_target_entry *entry; struct metal_frame_target *target = NULL;
			if (level) fputc(',', file);
			for (entry = render_targets; entry; entry = entry->next) if (at == &entry->target) { target = metal_frame_target(entry); break; }
			metal_frame_target_snapshot(target, FALSE); metal_frame_target_ref(file, target);
			if (!target) metal_frame.failed = TRUE;
		}
		fputc(']', file);
	}
	else
	{
		fputs(",\"payload\":", file);
		metal_frame_blob(file, PLATFORM_PHYSICAL_TO_VIRTUAL(texture->Data), xgpu_texture_face_size(&d) * (d.cube_map ? 6 : 1));
	}
	if (d.format == D3DFMT_P8)
	{
		D3DPalette *palette = device.palettes[stage]; fputs(",\"palette\":", file);
		if (!palette || !palette->Data) { fputs("null", file); metal_frame.failed = TRUE; }
		else metal_frame_blob(file, PLATFORM_PHYSICAL_TO_VIRTUAL(palette->Data), 1024);
	}
	fputc('}', file);
}

static void metal_frame_draw_begin(D3DPRIMITIVETYPE primitive, unsigned long first, unsigned long count,
	const WORD *indices, unsigned long index_count, BOOL immediate)
{
	FILE *file; unsigned long i; struct vertex_shader_object *decl = device.vertex_shader;
	if (!metal_frame_start()) return;
	file = metal_frame_record("draw", metal_frame.draw_count); if (!file) return;
	fprintf(file, "{\"schema_version\":1,\"kind\":\"frame_draw\",\"frame\":%lu,\"use\":%lu,\"program_id\":%lu,\"declaration_id\":%lu,\"primitive_d3d\":%lu,\"source_first_vertex\":%lu,\"vertex_count\":%lu,\"index_count\":%lu,\"source_base_vertex\":%lu,\"immediate\":%s,\"before\":", device.frame, metal_capture.use_count - 1, current_program()->id, decl->id, (unsigned long)primitive, first, count, index_count, (unsigned long)device.base_vertex_index, immediate ? "true" : "false");
	metal_frame_bound_refs(file, TRUE, FALSE);
	fputs(",\"vertex_constants\":", file); metal_frame_blob(file, device.constants, sizeof(device.constants));
	fputs(",\"draw_uniforms\":", file); metal_frame_blob(file, &draw_uniforms, sizeof(draw_uniforms));
	fputs(",\"fixed_attributes\":", file); metal_frame_blob(file, device.attributes, sizeof(device.attributes));
	fputs(",\"indices\":", file); if (indices) metal_frame_blob(file, indices, index_count * sizeof(WORD)); else fputs("null", file);
	fprintf(file, ",\"vertex_declaration\":{\"packed_mask\":%lu,\"elements\":[", immediate ? 0 : decl->packed_mask);
	for (i = 0; i < decl->element_count; i++) { struct vertex_element *e = &decl->elements[i]; fprintf(file, "%s{\"register\":%u,\"stream\":%u,\"offset\":%u,\"type\":%u}", i ? "," : "", e->reg, e->stream, e->offset, e->type); }
	fputs("]},\"streams\":[", file);
	if (immediate)
	{
		fputs("{\"stream\":0,\"stride\":256,\"source_first_vertex\":0,\"payload\":", file); metal_frame_blob(file, device.immediate_vertices, count * 256); fputc('}', file);
	}
	else
	{
		BOOL emitted = FALSE;
		for (i = 0; i < 16; i++)
		{
			unsigned long e; BOOL used = FALSE;
			for (e = 0; e < decl->element_count; e++) if (decl->elements[e].stream == i && decl->elements[e].type != D3DVSDT_NONE) used = TRUE;
			if (!used || !device.streams[i].data) continue;
			fprintf(file, "%s{\"stream\":%lu,\"stride\":%lu,\"source_first_vertex\":%lu,\"source_data\":%lu,\"payload\":", emitted ? "," : "", i, (unsigned long)device.streams[i].stride, first, (unsigned long)device.streams[i].data);
			metal_frame_blob(file, (const unsigned char *)PLATFORM_PHYSICAL_TO_VIRTUAL(device.streams[i].data) + first * device.streams[i].stride, device.streams[i].stride ? count * device.streams[i].stride : 64); fputc('}', file); emitted = TRUE;
		}
	}
	fputs("],", file); metal_frame_state(file);
	fputs("\"textures\":[", file); for (i = 0; i < 4; i++) { if (i) fputc(',', file); metal_frame_texture(file, i); }
	fprintf(file, "],\"visibility_test_active\":%s,\"complete\":%s}\n", device.visibility_test_active ? "true" : "false", metal_frame.failed ? "false" : "true");
	/* Atomic sample counts require another explicit resource kind. */
	if (device.visibility_test_active && xgpu_capabilities.atomic_counters) metal_frame.failed = TRUE;
	if (fclose(file)) metal_frame.failed = TRUE;
	metal_frame_event("draw"); fprintf(metal_frame.events, ",\"draw\":\"draw-%04lu.json\",\"use\":%lu,\"before\":", metal_frame.draw_count, metal_capture.use_count - 1); metal_frame_bound_refs(metal_frame.events, FALSE, FALSE);
}

static void metal_frame_draw_end(void)
{
	DWORD *rs = D3D__RenderState; BOOL readonly_query;
	if (!metal_frame.started || metal_frame.finished || !trace_frame() || !metal_frame.events) return;
	readonly_query = device.visibility_test_active && !rs[D3DRS_COLORWRITEENABLE] &&
		!rs[D3DRS_ZWRITEENABLE] && (!rs[D3DRS_STENCILENABLE] || !rs[D3DRS_STENCILWRITEMASK]);
	if (device.visibility_test_active && !readonly_query) metal_frame.failed = TRUE;
	fputs(",\"after\":", metal_frame.events); metal_frame_bound_refs(metal_frame.events, !device.visibility_test_active, !device.visibility_test_active);
	fprintf(metal_frame.events, ",\"readonly_visibility_query\":%s", readonly_query ? "true" : "false");
	metal_frame_event_end(); metal_frame.draw_count++;
}

static void metal_frame_visibility(const char *kind, unsigned long slot, unsigned long result)
{
	struct metal_frame_query *query;
	if (!metal_frame_start()) return;
	metal_frame_event(kind); fprintf(metal_frame.events, ",\"slot\":%lu,\"result\":%lu,\"atomic_counters\":%s", slot, result, xgpu_capabilities.atomic_counters ? "true" : "false");
	if (!strcmp(kind, "visibility_end"))
	{
		query = calloc(1, sizeof(*query));
		if (!query) metal_frame.failed = TRUE;
		else { query->query = device.queries[slot]; query->slot = slot; query->event = metal_frame.event_count - 1; query->next = metal_frame.queries; metal_frame.queries = query; }
		fprintf(metal_frame.events, ",\"gl_query\":%u", device.queries[slot]);
	}
	if (xgpu_capabilities.atomic_counters) metal_frame.failed = TRUE;
	metal_frame_event_end();
}

static void metal_frame_clear_begin(DWORD count, const D3DRECT *rectangles, DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
	DWORD i;
	if (!metal_frame_start()) return;
	metal_frame_event("clear"); fprintf(metal_frame.events, ",\"flags_d3d\":%lu,\"color_d3d\":%lu,\"depth\":%.9g,\"stencil\":%lu,\"viewport\":[%lu,%lu,%lu,%lu],\"rectangles\":[", flags, color, z, stencil, device.viewport.X, device.viewport.Y, device.viewport.Width, device.viewport.Height);
	for (i = 0; rectangles && i < count; i++) fprintf(metal_frame.events, "%s[%ld,%ld,%ld,%ld]", i ? "," : "", (long)rectangles[i].x1, (long)rectangles[i].y1, (long)rectangles[i].x2, (long)rectangles[i].y2);
	fputs("],\"before\":", metal_frame.events); metal_frame_bound_refs(metal_frame.events, TRUE, FALSE);
}

static void metal_frame_clear_end(void)
{
	if (!metal_frame.started || metal_frame.finished || !trace_frame() || !metal_frame.events) return;
	fputs(",\"after\":", metal_frame.events); metal_frame_bound_refs(metal_frame.events, TRUE, TRUE);
	metal_frame_event_end(); metal_frame.clear_count++;
}

static void metal_frame_copy(GLuint source, GLuint destination, GLint level, GLsizei width, GLsizei height, BOOL mipmap)
{
	if (!metal_frame_start()) return;
	metal_frame_event(mipmap ? "generate_mipmaps" : "texture_copy"); fprintf(metal_frame.events, ",\"source_gl_texture\":%u,\"destination_gl_texture\":%u,\"destination_level\":%d,\"width\":%d,\"height\":%d", source, destination, (int)level, (int)width, (int)height);
	/* Copy/mipmap GPU outputs must be captured/versioned by the alias importer.
 * Keep evidence and refuse a complete-frame claim until that is supported. */
	metal_frame.failed = TRUE; metal_frame.copy_count++; metal_frame_event_end();
}

static void metal_frame_finish(void)
{
	char path[1280], temporary[1280]; FILE *file;
	struct metal_frame_query *query;
	if (!trace_frame() || !metal_frame_start() || metal_frame.finished) return;
	for (query = metal_frame.queries; query; query = query->next)
	{
		GLuint available = 0, value = 0;
		glGetQueryObjectuiv(query->query, GL_QUERY_RESULT_AVAILABLE, &available);
		if (available) glGetQueryObjectuiv(query->query, GL_QUERY_RESULT, &value);
		else metal_frame.failed = TRUE;
		metal_frame_event("visibility_gpu_result"); fprintf(metal_frame.events, ",\"slot\":%lu,\"source_event\":%lu,\"gl_query\":%u,\"available\":%s,\"any_samples_passed\":%u", query->slot, query->event, query->query, available ? "true" : "false", value); metal_frame_event_end();
	}
	metal_frame_event("present"); fputs(",\"targets\":", metal_frame.events); metal_frame_bound_refs(metal_frame.events, TRUE, FALSE);
	fputs(",\"back_buffer\":", metal_frame.events);
	{ struct metal_frame_target *back = metal_frame_target(render_target_get(&device.back_buffer)); metal_frame_target_snapshot(back, FALSE); metal_frame_target_ref(metal_frame.events, back); }
	metal_frame_event_end();
	metal_frame.finished = TRUE;
	if (fclose(metal_frame.events)) metal_frame.failed = TRUE; metal_frame.events = NULL;
	snprintf(path, sizeof(path), "%s/status.json", metal_frame.directory); snprintf(temporary, sizeof(temporary), "%s/status.json.tmp", metal_frame.directory);
	file = fopen(temporary, "w"); if (!file) { metal_frame.failed = TRUE; return; }
	fprintf(file, "{\"schema_version\":1,\"kind\":\"captured_frame\",\"complete\":%s,\"frame\":%lu,\"event_count\":%lu,\"draw_count\":%lu,\"clear_count\":%lu,\"copy_count\":%lu,\"target_bind_count\":%lu,\"shader_corpus\":\"%s\",\"orientation\":\"top_left\",\"limits\":[\"Actual ANGLE Metal float32 depth storage; original physical Xbox depth quantization not established.\"]}\n", metal_frame.failed ? "false" : "true", device.frame, metal_frame.event_count, metal_frame.draw_count, metal_frame.clear_count, metal_frame.copy_count, metal_frame.bind_count, metal_capture.directory);
	if (fclose(file) || rename(temporary, path)) metal_frame.failed = TRUE;
	platform_log("METAL_FRAME_CAPTURE %s %lu events %lu draws %lu clears %s", metal_frame.failed ? "incomplete" : "complete", metal_frame.event_count, metal_frame.draw_count, metal_frame.clear_count, metal_frame.directory);
}
#endif
