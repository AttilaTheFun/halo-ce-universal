"""Check the production pose-change predicate against a roll-only remote turn."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <assert.h>
#include <math.h>
typedef float real;
typedef int boolean;
struct point { real x, y, z; };
struct vector { real i, j, k; };
struct drawn_pose {
    struct point latest_position;
    struct vector latest_forward, latest_up;
};
struct object_datum {
    struct { struct point position; struct vector forward, up; } object;
};
/* PRODUCTION */
int main(void) {
    struct drawn_pose pose = { {0,0,0}, {1,0,0}, {0,0,1} };
    struct object_datum object = { { {0,0,0}, {1,0,0}, {0,0,1} } };
    assert(!pose_changed(&pose, &object));
    /* Roll around an unchanged forward axis, with no translation. */
    object.object.up = (struct vector){0,1,0};
    assert(pose_changed(&pose, &object));
    object.object.up = pose.latest_up;
    object.object.forward = (struct vector){0,1,0};
    assert(pose_changed(&pose, &object));
    object.object.forward = pose.latest_forward;
    object.object.position.x = 1;
    assert(pose_changed(&pose, &object));
    object.object.position = pose.latest_position;
    object.object.up.k = NAN;
    assert(pose_changed(&pose, &object));
    return 0;
}
'''

class RemotePoseTests(unittest.TestCase):
    def test_rotation_includes_roll(self):
        source = (ROOT / 'port/linux/game/render_interpolation.c').read_text()
        start = source.index('static boolean pose_changed(')
        end = source.index('\n}\n', start) + 3
        with tempfile.TemporaryDirectory(prefix='halo-pose-test-') as directory:
            root = Path(directory)
            code = root / 'test.c'
            code.write_text(HARNESS.replace('/* PRODUCTION */', source[start:end]))
            binary = root / 'test'
            subprocess.run(['clang', '-std=c11', '-O2', '-fsanitize=address,undefined',
                            str(code), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

if __name__ == '__main__':
    unittest.main()
