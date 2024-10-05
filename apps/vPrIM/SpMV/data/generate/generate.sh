R="64"
make clean
make
for r in $R; do
    ./replicate ../bcsstk30.mtx $r ../bcsstk30.mtx.$r.mtx
done

