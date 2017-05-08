#!/bin/bash

# Step 1: open connLMLogo.key with Keynote, use Print and uncheck the
# `print slide backgrounds' to get connLMTextAndLogo.pdf

# Step 2: convert it to png with Imagemagick
convert -density 600 -trim connLMTextAndLogo.pdf -quality 100 -resize 200x55 connLMTextAndLogo.png

# Step 3: In Keynote, delete the text part and print it to connLMLogo.pdf

# Step 4: convert it to png with Imagemagick
convert -density 600 -trim connLMLogo.pdf -quality 100 -resize 50% connLMLogo.png

# Step 5: Create the favicon.ico
# command line from http://www.imagemagick.org/Usage/thumbnails/#favicon
convert connLMLogo.png -resize 256x256 -define icon:auto-resize="256,128,96,64,48,32,16" connLM.ico
