## Framework with a variety of Transactional Memory implementations and benchmarks ##

**Backends:**

* psi: DUMBO-SI
* psi-strong: DUMBO-opa
* htm-sgl: Power8 HTM used with a single global lock fallback
* si-htm: SI-HTM
* spht: SPHT

Notes: 
* besides the main backends listed above, other backends are also available in this repo.
* The pisces backend (Pisces STM) is available in from the power8tm-pisces folder.

**Benchmarks:**

* Concurrent data-structures (hashmap)
* TPCC on in-memory database


To compile the benchmarks, use there is a specialized script under each benchmarks folder. 
These scripts expect three parameters (in this order):

1. the backend name, corresponding to the folder under "backends"

2. the number of retries for HTM usage, which may be omitted (default value 5)

3. the number of retries for ROT usage after falling back from normal HTM (default value 2), must be >= 1 
   or in case of htm-sgl or hyrbid-norec: the retry policy to adopt in case of HTM persistent aborts, i.e., how the number of retries is updated upon a 
   persistent abort, which may be omitted (possible values: 0, do not retry in htm and resort to the fallback path; 1, decrease by
   one; 2, divide by two; default value 1)

Hence, a common usage shall be: "bash build-datastructures.sh psi 10 5"


