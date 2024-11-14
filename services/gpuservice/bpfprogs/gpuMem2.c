/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/types.h>
#include <bpf/bpf_helpers.h>
#include <linux/bpf.h>
#include <stdint.h>

/*
 * On Android the number of active processes using gpu is limited.
 * So this is assumed to be true: SUM(num_procs_using_gpu[i]) <= 1024
 */
#define GPU_MEM_TOTAL_MAP_SIZE 1024

/*
 * This map maintains the global and per process gpu memory total counters.
 *
 * The KEY is ((gpu_id << 32) | pid) while VAL is the size in bytes.
 * Use HASH type here since key is not int.
 * Pass AID_GRAPHICS as gid since gpuservice is in the graphics group.
 */
// GRO AID_GRAPHICS
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, uint64_t);
	__type(value, uint64_t);
	__uint(max_entries, GPU_MEM_TOTAL_MAP_SIZE);
} gpu_mem_total_map SEC(".maps");

/* This struct aligns with the fields offsets of the raw tracepoint format */
struct gpu_mem_total_args {
    uint64_t ignore;
    /* Actual fields start at offset 8 */
    uint32_t gpu_id;
    uint32_t pid;
    uint64_t size;
};
char _license[] SEC("license") = "Apache 2.0";
/*
 * This program parses the gpu_mem/gpu_mem_total tracepoint's data into
 * {KEY, VAL} pair used to update the corresponding bpf map.
 *
 * Pass AID_GRAPHICS as gid since gpuservice is in the graphics group.
 * Upon seeing size 0, the corresponding KEY needs to be cleaned up.
 */


// Owner: AID_ROOT
// Group: AID_GRAPHICS
SEC("tracepoint/gpu_mem/gpu_mem_total")
int tp_gpu_mem_total(struct gpu_mem_total_args* args) {
    uint64_t key = 0;
    uint64_t cur_val = 0;
    uint64_t* prev_val = NULL;

    /* The upper 32 bits are for gpu_id while the lower is the pid */
    key = ((uint64_t)args->gpu_id << 32) | args->pid;
    cur_val = args->size;

    if (!cur_val) {
        bpf_map_delete_elem(&gpu_mem_total_map, &key);
        return 0;
    }

    prev_val = bpf_map_lookup_elem(&gpu_mem_total_map, &key);
    if (prev_val) {
        *prev_val = cur_val;
    } else {
        bpf_map_update_elem(&gpu_mem_total_map, &key, &cur_val, BPF_NOEXIST);
    }
    return 0;
}
