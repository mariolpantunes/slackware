/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * File Name: gen8_scheduler_definitions.h
 *
 * Abstract:
 *
 * Notes: BDW specific definitions for scheduler kernel
 */
#define SIZEOF_INTERFACE_DESCRIPTOR_DATA_G8 32

// Generation dependent number
// Number of Interface Descriptors is 64 for BDW
#define NUMBER_OF_INERFACE_DESCRIPTORS 64
#define IDT_BREAKDOWN (NUMBER_OF_INERFACE_DESCRIPTORS - 2)

#define INTERFACE_DESCRIPTOR_TABLE_SIZE_G8 (NUMBER_OF_INERFACE_DESCRIPTORS * SIZEOF_INTERFACE_DESCRIPTOR_DATA_G8)
// Based on the alignment ( 64 vs 32 ) requirements this may be Gen dependent
#define SIZEOF_COLOR_CALCULATOR_STATE_G8 0xC0
#define OCLRT_SIZEOF_SAMPLER_STATE_G8 (16)

#define SIZEOF_COLOR_CALCULATOR_STATE SIZEOF_COLOR_CALCULATOR_STATE_G8
#define SIZEOF_INTERFACE_DESCRIPTOR_DATA SIZEOF_INTERFACE_DESCRIPTOR_DATA_G8
#define INTERFACE_DESCRIPTOR_TABLE_SIZE INTERFACE_DESCRIPTOR_TABLE_SIZE_G8
#define OCLRT_SIZEOF_SAMPLER_STATE OCLRT_SIZEOF_SAMPLER_STATE_G8

#define SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE (SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE_GEN8PLUS)
#define SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES (SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES_GEN8PLUS)

// DWORD OFFSET
#define MEDIA_STATE_FLUSH_INITIAL_OFFSET 0
//bits 0-5 of 1st DWORD
#define MEDIA_STATE_FLUSH_INITIAL_INTERFACE_DESCRIPTOR_OFFSET (MEDIA_STATE_FLUSH_INITIAL_OFFSET + 1)

#define MI_ATOMIC_CMD_OFFSET (MEDIA_STATE_FLUSH_INITIAL_OFFSET + OCLRT_SIZEOF_MSFLUSH_DWORD)

//#define OCLRT_MEDIA_VFE_STATE_OFFSET (MI_ATOMIC_CMD_OFFSET + OCLRT_SIZEOF_MI_ATOMIC_CMD_DWORD_OFFSET)
//address is QWORD in size and starts on DWORD 1
//#define MEDIA_VFE_STATE_ADDRESS_OFFSET (OCLRT_MEDIA_VFE_STATE_OFFSET + 1)

#define MEDIA_INTERFACE_DESCRIPTOR_LOAD_OFFSET (MI_ATOMIC_CMD_OFFSET + OCLRT_SIZEOF_MI_ATOMIC_CMD_DWORD_OFFSET) //(OCLRT_MEDIA_VFE_STATE_OFFSET + OCLRT_SIZEOF_MEDIA_VFE_STATE_DWORD)
// DWORD OFFSET of Interface Descriptor Start Address
#define MEDIA_INTERFACE_DESCRIPTOR_LOAD_INTERFACEDESCRIPTORSTARTADDRESS_OFFSET (MEDIA_INTERFACE_DESCRIPTOR_LOAD_OFFSET + 3)

#define INTERFACE_DESCRIPTOR_SAMPLER_STATE_TABLE_DWORD 3
#define INTERFACE_DESCRIPTOR_BINDING_TABLE_POINTER_DWORD 4
#define INTERFACE_DESCRIPTOR_CONSTANT_URB_ENTRY_READ_OFFSET 5
#define INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD 6
#define INTERFACE_DESCRIPTOR_SLMSIZE_DWORD 6
#define INTERFACE_DESCRIPTOR_HWTHREADS_UPPER_BIT 9

#define SAMPLER_STATE_INDIRECT_STATE_MASK (0x7FFFFC0)
#define SAMPLER_STATE_BORDER_COLOR_MASK (0xFFFFFFE0)
#define SAMPLER_STATE_DESCRIPTOR_BORDER_COLOR_POINTER_DWORD 2

//disable preemption is for Gen8
#if defined WA_LRI_COMMANDS_EXIST
#define IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET (MEDIA_INTERFACE_DESCRIPTOR_LOAD_OFFSET + OCLRT_SIZEOF_MEDIA_INTERFACE_DESCRIPTOR_LOAD_DEVICE_CMD_DWORD_OFFSET)
#define PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET (IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET + OCLRT_IMM_LOAD_REGISTER_CMD_DEVICE_CMD_DWORD_OFFSET)
#else
#define PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET (MEDIA_INTERFACE_DESCRIPTOR_LOAD_OFFSET + OCLRT_SIZEOF_MEDIA_INTERFACE_DESCRIPTOR_LOAD_DEVICE_CMD_DWORD_OFFSET)
#endif // WA_LRI_COMMANDS_EXIST

#define GPGPU_WALKER_OFFSET (PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G8_DWORD_OFFSET)
// DWORD OFFSET of the Interface Descriptor Offset for GPGPU_WALKER
// bits 0 - 5
#define GPGPU_WALKER_INTERFACE_DESCRIPTOR_ID_OFFSET (GPGPU_WALKER_OFFSET + 1)
// DWORD OFFSET of the Indirect data length Offset for GPGPU_WALKER
// bits 0 - 16
#define GPGPU_WALKER_INDIRECT_DATA_LENGTH_OFFSET (GPGPU_WALKER_OFFSET + 2)
// DWORD OFFSET of the Indirect Start Address for GPGPU_WALKER
#define GPGPU_WALKER_INDIRECT_START_ADDRESS_OFFSET (GPGPU_WALKER_OFFSET + 3)
// DWORD OFFSET of the Thread Width Counter Maximum for GPGPU_WALKER
// bits 0 - 5
#define GPGPU_WALKER_THREAD_WIDTH_DWORD (GPGPU_WALKER_OFFSET + 4)
// DWORD OFFSET of the Thread Height Counter Maximum for GPGPU_WALKER
// bits 8 - 13
#define GPGPU_WALKER_THREAD_HEIGHT_DWORD (GPGPU_WALKER_OFFSET + 4)
// DWORD OFFSET of the Thread Depth Counter Maximum for GPGPU_WALKER
// bits 16 - 21
#define GPGPU_WALKER_THREAD_DEPTH_DWORD (GPGPU_WALKER_OFFSET + 4)
// DWORD OFFSET of the SIMD Size for GPGPU_WALKER
// bits 30 - 31
#define GPGPU_WALKER_SIMDSIZE_DWORD (GPGPU_WALKER_OFFSET + 4)
// DWORD OFFSET of the Starting in X pos for GPGPU_WALKER
//bits 0 - 31
#define GPGPU_WALKER_GROUP_ID_START_X (GPGPU_WALKER_OFFSET + 5)
// DWORD OFFSET of the X Dimension for GPGPU_WALKER
#define GPGPU_WALKER_XDIM_DWORD (GPGPU_WALKER_OFFSET + 7)
// DWORD OFFSET of the Starting in Y pos for GPGPU_WALKER
//bits 0 - 31
#define GPGPU_WALKER_GROUP_ID_START_Y (GPGPU_WALKER_OFFSET + 8)
// DWORD OFFSET of the Y Dimension for GPGPU_WALKER
#define GPGPU_WALKER_YDIM_DWORD (GPGPU_WALKER_OFFSET + 10)
// DWORD OFFSET of the Starting in Z pos for GPGPU_WALKER
//bits 0 - 31
#define GPGPU_WALKER_GROUP_ID_START_Z (GPGPU_WALKER_OFFSET + 11)
// DWORD OFFSET of the X Dimension for GPGPU_WALKER
#define GPGPU_WALKER_ZDIM_DWORD (GPGPU_WALKER_OFFSET + 12)
// DWORD OFFSET of the Right or X Mask for GPGPU_WALKER
#define GPGPU_WALKER_XMASK_DWORD (GPGPU_WALKER_OFFSET + 13)
// DWORD OFFSET of the Bottom or Y Mask for GPGPU_WALKER
#define GPGPU_WALKER_YMASK_DWORD (GPGPU_WALKER_OFFSET + 14)

#define MEDIA_STATE_FLUSH_OFFSET (GPGPU_WALKER_OFFSET + OCLRT_GPGPU_WALKER_CMD_DEVICE_CMD_G8_DWORD_OFFSET)
//bits 0-5 of 1st DWORD of M_S_F command
#define MEDIA_STATE_FLUSH_INTERFACE_DESCRIPTOR_OFFSET (MEDIA_STATE_FLUSH_OFFSET + 1)

#define PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET (MEDIA_STATE_FLUSH_OFFSET + OCLRT_SIZEOF_MSFLUSH_DWORD)
#define PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET_TO_PATCH (PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET)
#define PIPE_CONTROL_POST_SYNC_DWORD 1
#define PIPE_CONTROL_POST_SYNC_START_BIT 14
#define PIPE_CONTROL_POST_SYNC_END_BIT 15
#define PIPE_CONTROL_GENERATE_TIME_STAMP 3
#define PIPE_CONTROL_NO_POSTSYNC_OPERATION 0
#define PIPE_CONTROL_ADDRESS_FIELD_DWORD 2
#define PIPE_CONTROL_PROFILING_START_TIMESTAMP_ADDRESS_OFFSET (PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET + PIPE_CONTROL_ADDRESS_FIELD_DWORD) //DWORD 2
#define PIPE_CONTROL_GRAPHICS_ADDRESS_START_BIT 2
#define PIPE_CONTROL_GRAPHICS_ADDRESS_END_BIT 31
#define PIPE_CONTROL_GRAPHICS_ADDRESS_HIGH_START_BIT 0
#define PIPE_CONTROL_GRAPHICS_ADDRESS_HIGH_END_BIT 15

#define PIPE_CONTROL_TIME_STAMP_DWORD0 0x7A000004
#define PIPE_CONTROL_TIME_STAMP_DWORD1 0x0010C4A4

#define PIPE_CONTROL_CSTALL_DWORD0 0x7A000004
#define PIPE_CONTROL_CSTALL_DWORD1 0x001004A4

#define PIPE_CONTROL_TAG_WRITE_DWORD0 0x7A000004
#define PIPE_CONTROL_TAG_WRITE_DWORD1 0x001044A4

// the value of InitMIBBStartCmd_G8 DWORD0
#define OCLRT_BATCH_BUFFER_BEGIN_CMD_DWORD0 (0x18800101)

#if defined WA_LRI_COMMANDS_EXIST
#define IMM_LOAD_REGISTER_FOR_ENABLE_PREEMPTION_OFFSET (PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G8_DWORD_OFFSET)
#endif

#define OCLRT_LOAD_REGISTER_IMM_CMD 0x11000001
#define CTXT_PREMP_DBG_ADDRESS_VALUE 0x2248
#define CTXT_PREMP_ON_MI_ARB_CHECK_ONLY 0x00000100
#define CTXT_PREMP_DEFAULT_VALUE 0x0

#define IMM_LOAD_REGISTER_ADDRESS_DWORD_OFFSET 1
#define IMM_LOAD_REGISTER_VALUE_DWORD_OFFSET 2
