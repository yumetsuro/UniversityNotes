#!/bin/zsh

# Check for .zip files and non-.zip files in the current directory
#check if no zip before
# check if no zip file
if [[ -z $(ls *.zip 2>/dev/null) ]]; then
  echo "No .zip files found in the current directory."
else
  echo "Found .zip files in the current directory."
  zip_files=(*.zip)
fi

# file different than .zip
non_zip_files=(*.*[^.zip])


# Calculate memory usage for .zip files
# if no zip file set zip_memory to 0 else count memory of zip
if [[ -z $zip_files ]]; then
  zip_memory=0
else
  zip_memory=$(du -cb "${zip_files[@]}" | grep total$ | awk '{print $1}')
fi

#for file in $zip_files; do
#  [[ -f $file ]] && zip_memory=$((zip_memory + $(stat -c%s "$file")))
#done

# Calculate memory usage for non-.zip files

for file in $non_zip_files; do
  [[ -f $file ]] && non_zip_memory=$((non_zip_memory + $(stat -c%s "$file")))
done

# Print results
echo "Memory usage for .zip files: $zip_memory bytes"
echo "Memory usage for non-.zip files: $non_zip_memory bytes"

# convert in kb or mb
zip_memory_kb=$((zip_memory / 1024))
non_zip_memory_kb=$((non_zip_memory / 1024))
zip_memory_mb=$((zip_memory / 1024 / 1024))
non_zip_memory_mb=$((non_zip_memory / 1024 / 1024))

# Print results in KB and MB
echo "Memory usage for .zip files: $zip_memory_kb KB ($zip_memory_mb MB)"
echo "Memory usage for non-.zip files: $non_zip_memory_kb KB ($non_zip_memory_mb MB)"
