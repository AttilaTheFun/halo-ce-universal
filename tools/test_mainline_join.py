"""Compile the actual pre-join announcement with a recording transport."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_private_capabilities_require_host_advertisement(tmp_path):
    client = (ROOT / 'source/networking/network_client_manager.c').read_text()
    start = client.index('word capability[NETWORK_PERFORMANCE_MESSAGE_SIZE / sizeof(word)];')
    end = client.index('csmemset(&join_game_request', start)
    announcement = client[start:end]
    fixture = r'''
#include <assert.h>
#include <stddef.h>
#include "networking/network_performance_protocol.h"
typedef unsigned char byte;
typedef unsigned short word;
#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define FALSE 0
enum { _performance_option_timer_audio = 4 };
struct client { void *connection; };
static int network_game_client_host_performance, audio, count;
static unsigned flags[3];
static int halo_performance_audio_available(void) { return audio; }
static int network_game_client_write(void *connection, void *message,
    unsigned size, void *address, int reliable) {
    assert(!connection && !address && reliable && count < 3);
    assert(network_performance_decode(message, size, NETWORK_PERFORMANCE_CAPABILITY, &flags[count]));
    count++;
    return 1;
}
static int announce(struct client *client) {
ANNOUNCEMENT
    return 1;
}
int main(void) {
    struct client client = {0};
    assert(announce(&client) && count == 0);
    network_game_client_host_performance = 1;
    assert(announce(&client) && count == 3);
    assert(flags[0] == 3 && flags[1] == 3 && flags[2] == 27);
    count = 0; audio = 1;
    assert(announce(&client) && count == 3);
    assert(flags[0] == 3 && flags[1] == 7 && flags[2] == 31);
    count = 0; network_game_client_host_performance = 0;
    assert(announce(&client) && count == 0);
    assert(!network_performance_version_compatible(0x800B, 4, 38));
    assert(network_performance_version_compatible(38, 0, 38));
}
'''.replace('ANNOUNCEMENT', announcement)
    source = tmp_path / 'join.c'
    binary = tmp_path / 'join'
    source.write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'source'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
