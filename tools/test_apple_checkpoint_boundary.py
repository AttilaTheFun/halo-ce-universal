#!/usr/bin/env python3
"""Run the production persistent-write function against faulting file operations."""
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'source/saved games/game_state_xbox.c').read_text()
function = source.split('void game_state_write_to_persistent_storage(', 1)[1].split('\nvoid game_state_read_from_persistent_storage(', 1)[0]
function = 'void game_state_write_to_persistent_storage(' + function
prefix = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define HALO_MACOS 1
#define GAME_STATE_SIZE 32
#define INVALID_HANDLE_VALUE -1
#define INVALID_SET_FILE_POINTER -1
#define FILE_BEGIN 0
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
typedef unsigned char byte;
typedef int boolean;
typedef int HANDLE;
static int fault, writes, closed, captured;
static char temporary[256];
static int game_state_open_persistent_storage(void *p) {(void)p;return fault == 3 ? -1 : 1;}
static void crc_new(unsigned long *x) {*x=0;}
static void crc_checksum_buffer(unsigned long *x, void *b, int n) {(void)b;(void)n;*x=42;}
static void match_assert(const char *f,int n,int ok) {(void)f;(void)n;assert(ok);}
static int SetFilePointer(int f,int o,void *p,int w) {(void)f;(void)o;(void)p;(void)w;return 0;}
static int WriteFile(int f,void *b,unsigned long n,unsigned long *out,void *p) {
 (void)f;(void)b;(void)p;writes++;*out=n;return !(fault==writes);
}
static int GetLastError(void) {return 1;}
#define csprintf snprintf
static void match_vassert(const char *f,int n,int ok,int detail) {(void)f;(void)n;(void)ok;(void)detail;}
static void delete_persistent_storage(void) {}
static void CloseHandle(int f) {(void)f;closed=1;}
void halo_save_checkpoint_committed(void) {assert(closed && writes==2 && !fault);captured++;}
'''
# csprintf returns a string in the game; the assertion logger is inert here.
prefix = prefix.replace('#define csprintf snprintf', '#define csprintf(...) 0')
suffix = r'''
int main(void) {
 for(fault=0;fault<=3;fault++) {
  unsigned char data[32]={0}; unsigned long checksum=0;
  writes=closed=captured=0;
  game_state_write_to_persistent_storage(data,&checksum,8,sizeof(data));
  assert(captured==(fault==0));
 }
 puts("Persistent-write boundary: completed/closed saves captured; failed payload/header/open never captured");
}
'''
out = ROOT / 'build/apple-tests/checkpoint-boundary'
out.parent.mkdir(parents=True, exist_ok=True)
unit = out.with_suffix('.c'); unit.write_text(prefix + function + suffix)
subprocess.run(['clang','-fsanitize=address,undefined','-Wno-macro-redefined',str(unit),'-o',str(out)],check=True)
subprocess.run([str(out)],check=True)
