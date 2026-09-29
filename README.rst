## **Main‑tiedostot ja pisteytys**

Projektissa on kolme erillistä `main()`‑tiedostoa, jotka vastaavat eri pisteytystä:

- `main.c`      = 1p suoritus
- `main2p.c`    = 2p suoritus
- `main3p.c`    = 3p suoritus

---

## **CMake‑asetusten muokkaus**

Jotta oikea `main`‑tiedosto toimii, sinun täytyy vaihtaa `CMakeLists.txt`‑tiedostossa käytettävä lähdekoodi.

Etsi rivi:

```cmake
target_sources(app PRIVATE src/main.c)
```

Vaihda se haluamaasi tiedostoon:

```cmake
target_sources(app PRIVATE src/main2p.c)
```

tai

```cmake
target_sources(app PRIVATE src/main3p.c)
```


