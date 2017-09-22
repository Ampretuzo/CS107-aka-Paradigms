echo -e "\n    -- running clean --"
make clean
echo -e "\n    -- running default make task --"
make
echo -e "\n    -- running resulting file --"
./rss-news-search
