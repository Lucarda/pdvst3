@echo off
del pdvst3-%1.zip /Q
7z a pdvst3-%1.zip pdvst3.vst3
@echo on
