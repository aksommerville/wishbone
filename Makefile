all:
.SILENT:

ifeq (,$(EGG2_SDK))
  EGG2_SDK:=../egg2
endif
EGGDEV:=$(EGG2_SDK)/out/eggdev

all:;$(EGGDEV) build
clean:;rm -rf mid out
run:;$(EGGDEV) run
web-run:all;$(EGGDEV) serve --htdocs=out/wishbone-web.zip --project=.
edit:;$(EGGDEV) serve \
  --htdocs=/build:out/wishbone-web.zip \
  --htdocs=/synth.wasm:EGG_SDK/out/web/synth.wasm \
  --htdocs=/data:src/data \
  --htdocs=EGG_SDK/src/web \
  --htdocs=EGG_SDK/src/editor \
  --htdocs=src/editor \
  --htdocs=/out:out \
  --writeable=src/data \
  --project=.
