gcc -o mem_sim mem_sim.c -std=gnu99 -lm -g
python3 exhauster.py
diff exhaustive-report.txt exhaustive-pedro.txt &> diff.log
wc -l diff.log