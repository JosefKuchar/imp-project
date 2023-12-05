# IMP Projekt

Dokumentace se nachází v souboru `doc.pdf`.

## Vytvoření dokumentace

Dokumentace je napsána v Markdownu a přeložena do PDF pomocí [Pandocu](https://pandoc.org/).

```
pandoc -V lang=cs -V linkcolor=blue -V urlcolor=blue -V block-headings -N doc.md -o doc.pdf
```
