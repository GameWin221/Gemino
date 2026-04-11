from numpy.random import uniform
import numpy as np
import itertools as it

FLOAT_PRECISION = 6

NUM_KERNELS = 64
NUM_KERNELS_PER_LINE = 4

NUM_ROTATIONS = 16
NUM_ROTATIONS_PER_LINE = 4

def normalized(v):
    return v / np.sqrt(np.sum(v ** 2))

def lerp(a, b, v):
    return a + v * (b - a)

# scale should be in range [0, 1)
def gen_kernel(scale: float):
    sample = normalized(np.array((uniform(-1.0, 1.0), uniform(-1.0, 1.0), uniform(0.0, 1.0))))
    sample *= uniform(0.0, 1.0)
    sample *= lerp(0.1, 1.0, scale * scale)

    return sample

def gen_rotation():
    sample = (uniform(-1.0, 1.0), uniform(-1.0, 1.0), 0.0)
    
    return sample
    #return normalized(sample)

def print_vec3_array(data, name: str, elems_per_line: int, float_precision: int):
    data_len = len(data)
    
    print(f"const vec3 {name}[{data_len}] = vec3[{data_len}](")
    for (i, batch) in enumerate(it.batched(data, elems_per_line)):
        print("    ", end='')
        for (j, kernel) in enumerate(batch):
            print(f"vec3({kernel[0]:.{float_precision}f}, {kernel[1]:.{float_precision}f}, {kernel[2]:.{float_precision}f})", end='')

            if i * elems_per_line + j != data_len - 1:
                print(", ", end='')

        print('')
    print(");")

# Kernels are uniformly distributed in a normal-oriented hemi-sphere
kernels = [gen_kernel(i / NUM_KERNELS) for i in range(NUM_KERNELS)]
rotations = [gen_rotation() for _ in range(NUM_ROTATIONS)]

print_vec3_array(kernels, "KERNELS", NUM_KERNELS_PER_LINE, FLOAT_PRECISION)
print_vec3_array(rotations, "ROTATIONS", NUM_ROTATIONS_PER_LINE, FLOAT_PRECISION)