project('RTOS', ['c', 'cpp'], default_options : ['c_std=c11', 'cpp_std=c++14', 'b_staticpic=false'])


#================================ Settings ==========================================

compiler_optimization = 'Os'

preprocessor_tags = [
    'DEBUG',
    #'TX_NO_ASSERT',
    'STM32',
    'STM32F2',
    'STM32F207GZTx',
    ]

size    = 'arm-none-eabi-size'
objdump = 'arm-none-eabi-objdump'
objcopy = 'arm-none-eabi-objcopy'

out_name = 'RTOS'

mode_args = [
    '-mcpu=@0@'.format(host_machine.cpu() == 'cortex-m0+' ? 'cortex-m0plus' : host_machine.cpu()),
    '-mthumb',
    '-mfloat-abi=soft',
    '--specs=nano.specs',
	#'--print-file-name=libc.a',
	]



#===================================== Compiler arguments ==========================

c_compiler_args = [   # default arguments
    '-fdata-sections',          # each variable on a seperate section
    '-ffunction-sections',      # each function on a seperate section
    '-fstack-usage',

    '-Wall',
    '-g3',
    '-@0@'.format(compiler_optimization),
    ]

foreach preprocessor_tag : preprocessor_tags
    c_compiler_args += ['-D@0@'.format(preprocessor_tag)]
endforeach

cpp_compiler_args = c_compiler_args + [
    '-fno-exceptions',          # remove exception handling code
    '-fno-rtti',                # disable dynamic polymorphism
    '-fno-use-cxa-atexit',		# do not call destructors in atexit
    ]




#===================================== Linker arguments ==========================

linker_args = [     # default arguments
    '-Wl,--gc-sections',    # Remove unused sections
    '-static',          # Do not link against shared libraries
    #'-Wl,-Map,@0@.map'.format(out_name),
    ]  




#======================== Source files and include directories ====================

source_files = []
header_directories = []

subdir('./Source/Kernel')
subdir('./External/CMSIS')
subdir('./External/MyLib')




#============================= Compilation flow ===================================

out = static_library('@0@'.format(out_name),
            source_files,
            c_args              : [mode_args, c_compiler_args],
            cpp_args            : [mode_args, cpp_compiler_args],
            include_directories : header_directories
            )

custom_target(
            'object dump',
            build_by_default : true,
            capture : true,
            output : ['@0@.list'.format(out_name)],
            command : [objdump, '-h', '-S', 'lib@0@.a'.format(out_name)],
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

