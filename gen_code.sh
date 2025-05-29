#!/bin/sh

echo -e "== File Tree\n\`\`\`\nMakefile\n$(tree src include)\n\`\`\`" > code.typ

find Makefile src include \( -name "Makefile" -o -name "*.c" -o -name "*.h" -o -name "*.js" -o -name "*.css" -o -name "*.html" -o -name "*.ts" \) -not -path '*/sodium.js' -type f -print0 | while IFS= read -r -d '' file;
do
	ext="${file##*.}"
	filename=$(basename -- "$file")
	filename="${filename%.*}"
	# replace filename every / with - then add extension for tag
	tag="code-$(echo $filename | tr / -)-$ext"
	if [ $ext == "Makefile" ];then
		tag="code-Makefile"
	fi
    echo "== $file <$tag>"
	echo "\`\`\`$ext"
    cat "$file"
    echo "\`\`\`"
done >> code.typ
mv code.typ doc
