# Seeds

Utility to generate the seeds.txt list that is compiled into the client
(see [src/chainparamsseeds.h](/src/chainparamsseeds.h) and other utilities in [contrib/seeds](/contrib/seeds)).

Be sure to update `PATTERN_AGENT` in `makeseeds.py` to include the current version,
and remove old versions as necessary (at a minimum when GetDesirableServiceFlags
changes its default return value, as those are the services which seeds are added
to addrman with).

The seeds compiled into the release are created from sipa's DNS seed and AS map
data. Run the following commands from the `/contrib/seeds` directory:

```
curl -s http://1seed.sugarchain.info | gzip -dc > seeds_main.txt
curl -s http://1seed-testnet.cryptozeny.com | gzip -dc > seeds_main.txt
python3 makeseeds.py < seeds_main.txt > nodes_main.txt
python3 makeseeds.py < seeds_test.txt > nodes_test.txt
python3 generate-seeds.py . > ../../src/chainparamsseeds.h
```
