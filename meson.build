project('RTOS', ['c', 'cpp'], default_options : ['c_std=c11', 'cpp_std=c++14'])


#================================ User settings =====================================

compiler_optimization = 'Os'

preprocessor_tags = [
    'DEBUG',
	'TX_NO_ASSERT',
    'STM32',
    'STM32F2',
    'STM32F207GZTx',
    ]

size    = 'arm-none-eabi-size'
objdump = 'arm-none-eabi-objdump'
objcopy = 'arm-none-eabi-objcopy'

out_name = 'RTOS'





#===================================== Compiler arguments ==========================

compiler_args = [   # default arguments
    '-mthumb',
    '-mfloat-abi=soft',
    '--specs=nano.specs',

    '-fdata-sections',          # each variable on a seperate section
    '-ffunction-sections',      # each function on a seperate section
    '-fstack-usage',

    '-Wall',
    '-g3', 
    ]

cpu = host_machine.cpu() == 'cortex-m0+' ? 'cortex-m0plus' : host_machine.cpu()
compiler_args = ['-mcpu=@0@'.format(cpu)] + compiler_args

compiler_args += ['-@0@'.format(compiler_optimization)]

foreach preprocessor_tag : preprocessor_tags
    compiler_args += ['-D@0@'.format(preprocessor_tag)]
endforeach

cpp_compiler_args = [
    '-fno-exceptions',          # remove exception handling code
    '-fno-rtti',                # disable dynamic polymorphism
    '-fno-use-cxa-atexit',
    ]




#===================================== Linker arguments ==========================

linker_args = [     # default arguments
    '-Wl,--gc-sections',    # Remove unused sections
    '-static',          # Do not link against shared libraries
    '-Wl,-Map,@0@.map'.format(out_name),
    ]  

#linker_args += ['-Wl,-T,@0@/@1@'.format(meson.current_source_dir(), linker_script)]




#======================== Source files and include directories ====================

source_files = []
header_directories = []

subdir('./Source/Kernel')
subdir('./External/CMSIS')
subdir('./External/MyLib')




#============================= Compilation flow ===================================

out = static_library('@0@'.format(out_name),
            source_files,
            c_args              : compiler_args,
            cpp_args            : [compiler_args, cpp_compiler_args],
            #link_args           : linker_args,
            #link_depends        : linker_script,
            include_directories : header_directories
            )

custom_target(
            'object dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.list'.format(out_name)],
            command : [objdump, '-x', '-S', 'lib@0@.a'.format(out_name)],
            depends : [out])

custom_target(
            'size dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.size'.format(out_name)],
            command : [size, 'lib@0@.a'.format(out_name)],
            depends : [out])



#=============================== Compile examples =================================

example_dep = declare_dependency(
            link_with : out,
            )

example_includes = ['External/CMSIS/Include', 'External/CMSIS/Device/ST/STM32F2xx/Include']

subdir('./Examples/Example1')

