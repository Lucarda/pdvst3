#!/bin/bash

rm -f pdvst3-$1.zip
zip -r -q pdvst3-$1.zip pdvst3.vst3
rm -r pdvst3.vst3
