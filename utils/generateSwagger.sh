#!/bin/bash

python \
  `pwd`/utils/generateSwagger.py \
  `pwd` \
  `pwd`/js/apps/system/_admin/aardvark/APP/api-docs \
  api-docs \
  `pwd`/Documentation/DocuBlocks/Rest \
  > `pwd`/js/apps/system/_admin/aardvark/APP/api-docs.json
