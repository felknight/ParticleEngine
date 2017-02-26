all: libpengine.so example

libpengine.so: ParticleEngine.cpp ParticleEngine_Sizes.hpp vert.o frag.o makefile ParticleEngine_Configs.hpp
	g++ -O3 -shared -fPIC -o libpengine.so.1.0.0 ParticleEngine.cpp vert.o frag.o -lvulkan -lIL -lm -lxcb -lxcb-icccm
	strip libpengine.so.1.0.0
	ln -sf libpengine.so.1.0.0 libpengine.so

example: Example.cpp makefile
	g++ -O2 -o example Example.cpp -I. -L. -lpengine

vert.o: particle.vert
	glslangValidator -V particle.vert
	objcopy -I binary -O elf64-x86-64 -B i386 vert.spv vert.o

frag.o: particle.frag
	glslangValidator -V particle.frag
	objcopy -I binary -O elf64-x86-64 -B i386 frag.spv frag.o

ParticleEngine_Sizes.hpp: vert.o frag.o makefile
	rm -f ParticleEngine_Sizes.hpp
	touch ParticleEngine_Sizes.hpp
	nm vert.o | grep _binary_vert_spv_size | awk '{print "#define VERT_SIZE","0x"$$1}' >> ParticleEngine_Sizes.hpp
	nm frag.o | grep _binary_frag_spv_size | awk '{print "#define FRAG_SIZE","0x"$$1}' >> ParticleEngine_Sizes.hpp


