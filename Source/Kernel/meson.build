local_source_files = [
	'rtos_impl.cpp', 
	'rtos_profiler.cpp', 
	'rtos_scheduler.cpp', 
	'rtos_system_timer.cpp',
	]
	
foreach local_source_file : local_source_files
    source_files += ['@0@/@1@'.format(meson.current_source_dir(), local_source_file)]
endforeach
