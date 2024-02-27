local_out_name = 'example1'
local_linker_script = 'STM32F207ZGTX_FLASH.ld'

local_linker_args = linker_args + '-Wl,-T,@0@/@1@'.format(meson.current_source_dir(), local_linker_script)

local_includes = ['../..']
foreach example_include : example_includes
	local_includes += ['../../@0@'.format(example_include)]
endforeach

local_sources_files = ['main.cpp', 'startup_stm32f207zgtx.s']
#foreach source_file : source_files
#	local_sources_files += ['../../@0@'.format(source_file)]
#endforeach

local_exec = executable('@0@.elf'.format(local_out_name),
            [local_sources_files, source_files],
            c_args              : compiler_args,
            cpp_args            : [compiler_args, cpp_compiler_args],
            #dependencies        : example_dep,
            link_args           : local_linker_args,
            link_depends        : local_linker_script,
            include_directories : local_includes,
            )
			
custom_target(
            'object dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.list'.format(local_out_name)],
            command : [objdump, '-h', '-S', '@0@/@1@.elf'.format(meson.current_build_dir(), local_out_name)],
            depends : [out])
			
local_map = custom_target(
            'memory dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.temp'.format(local_out_name)],
            command : [objdump, '-t', '@0@/@1@.elf'.format(meson.current_build_dir(), local_out_name)],
            depends : [out])
			
custom_target(
            'memory dump sort',
            build_by_default : true,
            capture : true,
            output : ['@0@.map'.format(local_out_name)],
            command : ['sort', '@0@/@1@.temp'.format(meson.current_build_dir(), local_out_name)],
            depends : [local_map])
			
custom_target(
		    'size dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.size'.format(local_out_name)],
            command : [size, '@0@/@1@.elf'.format(meson.current_build_dir(), local_out_name)],
            depends : [local_exec])