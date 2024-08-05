
cp ../../backends/$1/nvhtm/src/global_structs.c $2
cp ../../backends/$1/nvhtm/src/impl_pcwm.c $2
cp ../../backends/$1/nvhtm/src/impl_pcwm2.c $2
cp ../../backends/$1/nvhtm/src/impl_crafty.c $2
cp ../../backends/$1/nvhtm/src/impl_ccHTM.c $2
cp ../../backends/$1/nvhtm/src/impl_epoch_impa.c $2
cp ../../backends/$1/nvhtm/src/impl_PHTM.c $2
cp ../../backends/$1/nvhtm/src/impl_htmOnly.c $2
cp ../../backends/$1/nvhtm/src/impl_log_replayer.cpp $2
cp ../../backends/$1/nvhtm/src/containers.cpp $2
cp ../../backends/$1/nvhtm/src/spins.cpp $2

cp ../../backends/$1/nvhtm/include/array_utils.h $2
cp ../../backends/$1/nvhtm/include/containers.h $2
cp ../../backends/$1/nvhtm/include/global_structs.h $2
cp ../../backends/$1/nvhtm/include/impl.h $2
cp ../../backends/$1/nvhtm/include/htm_impl.h $2
cp ../../backends/$1/nvhtm/include/spins.h $2

cp ../../backends/$1/deps/input_handler/include/input_handler.h $2
cp ../../backends/$1/deps/input_handler/src/input_handler.cpp $2

cp ../../backends/$1/deps/threading/include/threading.h $2
cp ../../backends/$1/deps/threading/src/threading.cpp $2
cp ../../backends/$1/deps/threading/src/prod-cons.h $2
cp ../../backends/$1/deps/threading/src/prod-cons.cpp $2
cp ../../backends/$1/deps/threading/src/util.h $2

cp ../../backends/$1/deps/arch_dep/include/rdtsc.h $2
cp ../../backends/$1/deps/arch_dep/include/htm_arch.h $2

cp ../../backends/$1/deps/htm_alg/include/htm_retry_template.h $2
cp ../../backends/$1/deps/htm_alg/src/htm.cpp $2
