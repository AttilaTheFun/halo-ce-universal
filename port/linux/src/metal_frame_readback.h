extern void hostgl_glGetFloatv(GLenum name, GLfloat *values);
/* Isolated capture readback derived from the verified opaque fixture.
 * All diagnostic draws target separate color storage; source depth/stencil
 * write masks stay zero. Every modified game GL state is restored. */
static BOOL metal_frame_readback_write(const char *directory,const char *name,const void *data,unsigned long bytes)
{
 char path[1280]; FILE *file; BOOL ok;
 snprintf(path,sizeof(path),"%s/%s",directory,name);file=fopen(path,"wb");if(!file)return FALSE;
 ok=fwrite(data,1,bytes,file)==bytes;if(fclose(file))ok=FALSE;return ok;
}
/* ANGLE GLES lacks direct depth/stencil ReadPixels. Read the actual GPU values
 * without modifying those attachments: sample depth and bit-test stencil. */
static GLuint metal_frame_readback_program(const char *fragment)
{
	const char *vertex="#version 300 es\nprecision highp float;\nvoid main(){vec2 p=gl_VertexID==0?vec2(-1,-1):(gl_VertexID==1?vec2(3,-1):vec2(-1,3));gl_Position=vec4(p,0,1);}\n";
	GLuint vs=compile_shader(GL_VERTEX_SHADER,vertex,"readback fullscreen"),fs=compile_shader(GL_FRAGMENT_SHADER,fragment,"readback attachment"),program;
	GLint linked=0;if(!vs || !fs)return 0;
	program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glGetProgramiv(program,GL_LINK_STATUS,&linked);
	glDeleteShader(vs);glDeleteShader(fs);if(!linked)return 0;return program;
}

static BOOL metal_frame_gpu_readback(const char *directory,const char *phase,
	unsigned long width,unsigned long height,GLuint depth_texture)
{
	static GLuint depth_program,stencil_program,read_sampler;
	const char *depth_fragment="#version 300 es\nprecision highp float;precision highp int;uniform highp sampler2D sourceDepth;out vec4 result;void main(){uint b=floatBitsToUint(texelFetch(sourceDepth,ivec2(gl_FragCoord.xy),0).r);result=vec4(uvec4(b&255u,(b>>8u)&255u,(b>>16u)&255u,(b>>24u)&255u))/255.0;}\n";
	const char *stencil_fragment="#version 300 es\nprecision highp float;uniform float bitValue;out vec4 result;void main(){result=vec4(bitValue,0,0,0);}\n";
	GLint read_fbo,draw_fbo,program,active,texture0,sampler0,depth_enable,depth_write,stencil_enable,stencil_func,stencil_ref,stencil_mask,stencil_write,stencil_fail,stencil_zfail,stencil_zpass,blend_enable,blend_src,blend_dst,blend_eq,cull_enable,scissor_enable,color_mask[4];
	GLuint color_texture=0,framebuffer=0,control_texture=0;unsigned char *pixels,*stencil;unsigned long i;char path[1024],filename[128];FILE *file;BOOL ok=TRUE,control_valid=FALSE;GLenum error;GLint stencil_clear,viewport[4],offset_fill; GLfloat clear_color[4];
	if(!depth_program)depth_program=metal_frame_readback_program(depth_fragment);
	if(!stencil_program)stencil_program=metal_frame_readback_program(stencil_fragment);
	if(!depth_program || !stencil_program)return FALSE;
	if(!read_sampler){glGenSamplers(1,&read_sampler);glSamplerParameteri(read_sampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glSamplerParameteri(read_sampler,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glSamplerParameteri(read_sampler,GL_TEXTURE_COMPARE_MODE,GL_NONE);}
	pixels=malloc(width*height*4);stencil=malloc(width*height);if(!pixels || !stencil){free(pixels);free(stencil);return FALSE;}
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);glGetIntegerv(GL_CURRENT_PROGRAM,&program);
	glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);glGetIntegerv(GL_SAMPLER_BINDING,&sampler0);
	glGetIntegerv(GL_DEPTH_TEST,&depth_enable);glGetIntegerv(GL_DEPTH_WRITEMASK,&depth_write);glGetIntegerv(GL_STENCIL_TEST,&stencil_enable);
	glGetIntegerv(GL_STENCIL_FUNC,&stencil_func);glGetIntegerv(GL_STENCIL_REF,&stencil_ref);glGetIntegerv(GL_STENCIL_VALUE_MASK,&stencil_mask);glGetIntegerv(GL_STENCIL_WRITEMASK,&stencil_write);
	glGetIntegerv(GL_STENCIL_FAIL,&stencil_fail);glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL,&stencil_zfail);glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS,&stencil_zpass);
	glGetIntegerv(GL_STENCIL_CLEAR_VALUE,&stencil_clear);
	glGetIntegerv(GL_VIEWPORT,viewport);hostgl_glGetFloatv(GL_COLOR_CLEAR_VALUE,clear_color);glGetIntegerv(GL_POLYGON_OFFSET_FILL,&offset_fill);
	glGetIntegerv(GL_BLEND,&blend_enable);glGetIntegerv(GL_BLEND_SRC_RGB,&blend_src);glGetIntegerv(GL_BLEND_DST_RGB,&blend_dst);glGetIntegerv(GL_BLEND_EQUATION_RGB,&blend_eq);
	glGetIntegerv(GL_CULL_FACE,&cull_enable);glGetIntegerv(GL_SCISSOR_TEST,&scissor_enable);glGetIntegerv(GL_COLOR_WRITEMASK,color_mask);
	glGenTextures(1,&color_texture);glBindTexture(GL_TEXTURE_2D,color_texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
	glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color_texture,0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)ok=FALSE;
	glViewport(0,0,width,height);glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_STENCIL_TEST);glStencilMask(0);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	if(ok){
		/* This FBO has no depth attachment, avoiding sampler feedback. */
		glBindTexture(GL_TEXTURE_2D,depth_texture);glBindSampler(0,read_sampler);glUseProgram(depth_program);glUniform1i(glGetUniformLocation(depth_program,"sourceDepth"),0);
		glDrawArrays(GL_TRIANGLES,0,3);glFinish();glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
		snprintf(filename,sizeof(filename),"%s-depth.float32",phase);ok &= metal_frame_readback_write(directory,filename,pixels,width*height*4);
		/* Stencil tests read the same attachment; all write masks are zero. */
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_2D,depth_texture,0);
		if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)ok=FALSE;
		glUseProgram(stencil_program);glBindTexture(GL_TEXTURE_2D,0);glEnable(GL_STENCIL_TEST);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glBlendEquation(GL_FUNC_ADD);
		glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
		for(i=0;i<8;i++){glStencilFunc(GL_NOTEQUAL,0,1U<<i);glUniform1f(glGetUniformLocation(stencil_program,"bitValue"),(float)(1U<<i)/255.f);glDrawArrays(GL_TRIANGLES,0,3);}
		glFinish();glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
		for(i=0;i<width*height;i++)stencil[i]=pixels[i*4];
		snprintf(filename,sizeof(filename),"%s-stencil.uint8",phase);ok &= metal_frame_readback_write(directory,filename,stencil,width*height);
		/* Validate all eight readback bits against a separate known165 control
		 * attachment; never clear or write the original captured attachment. */
		glGenTextures(1,&control_texture);glBindTexture(GL_TEXTURE_2D,control_texture);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH24_STENCIL8,width,height,0,GL_DEPTH_STENCIL,GL_UNSIGNED_INT_24_8,NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_TEXTURE_2D,control_texture,0);glStencilMask(255);glClearStencil(165);glClear(GL_STENCIL_BUFFER_BIT);glStencilMask(0);glClear(GL_COLOR_BUFFER_BIT);
		for(i=0;i<8;i++){glStencilFunc(GL_NOTEQUAL,0,1U<<i);glUniform1f(glGetUniformLocation(stencil_program,"bitValue"),(float)(1U<<i)/255.f);glDrawArrays(GL_TRIANGLES,0,3);}
		glFinish();glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);control_valid=TRUE;
		for(i=0;i<width*height;i++){stencil[i]=pixels[i*4];if(stencil[i]!=165)control_valid=FALSE;}
		snprintf(filename,sizeof(filename),"%s-control-stencil.uint8",phase);ok &= metal_frame_readback_write(directory,filename,stencil,width*height);ok &= control_valid;
	}
	error=glGetError();if(error){ok=FALSE;platform_log("GPU_ATTACHMENT_READBACK %s error %lx",phase,(unsigned long)error);}
	/* Restore every changed pipeline/binding input without touching caches. */
	glUseProgram((GLuint)program);glBindSampler(0,(GLuint)sampler0);glBindTexture(GL_TEXTURE_2D,(GLuint)texture0);glActiveTexture((GLenum)active);
	glDepthMask(depth_write);if(depth_enable)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
	glStencilFunc(stencil_func,stencil_ref,(GLuint)stencil_mask);glStencilMask((GLuint)stencil_write);glStencilOp(stencil_fail,stencil_zfail,stencil_zpass);if(stencil_enable)glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);
	glClearStencil(stencil_clear);glClearColor(clear_color[0],clear_color[1],clear_color[2],clear_color[3]);
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(offset_fill)glEnable(GL_POLYGON_OFFSET_FILL);else glDisable(GL_POLYGON_OFFSET_FILL);
	glBlendFunc(blend_src,blend_dst);glBlendEquation(blend_eq);if(blend_enable)glEnable(GL_BLEND);else glDisable(GL_BLEND);
	if(cull_enable)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);if(scissor_enable)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);
	glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)read_fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,(GLuint)draw_fbo);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&color_texture);if(control_texture)glDeleteTextures(1,&control_texture);
	free(pixels);free(stencil);
	snprintf(path,sizeof(path),"%s/%s-gpu-readback.json",directory,phase);file=fopen(path,"w");if(!file)return FALSE;
	fprintf(file,"{\"schema_version\":1,\"complete\":%s,\"phase\":\"%s\",\"orientation\":\"logical_xbox_top_left\",\"depth_method\":\"GPU texelFetch depth floatBitsToUint encoded into RGBA8 little-endian bytes; nearest non-mipmap sampler\",\"stencil_method\":\"Eight read-only stencil bit tests encoded additively into RGBA8 red byte\",\"stencil_control_value\":165,\"stencil_control_valid\":%s,\"depth_write\":false,\"stencil_write_mask\":0,\"gl_error\":%lu}\n",ok?"true":"false",phase,control_valid?"true":"false",(unsigned long)error);
	if(fclose(file)!=0)ok=FALSE;platform_log("GPU_ATTACHMENT_READBACK %s complete %d",phase,ok);return ok;
}
