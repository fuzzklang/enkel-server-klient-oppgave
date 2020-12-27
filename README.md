Hjemmeeksamen i introduksjonsfag til operativsystemer og nettverk.

**OBS!** På grunn av opphavsrett kan ikke oppgaveteksten og utleverte filer publiseres. **Programmet er derfor ikke kjørbart (mangler bl.a. gyldige input-filer).**

Et par av funksjonene er handouts og er derfor blitt fjernet fra kildekoden.

Setter opp en enkel server og klient. Klienten sender filer (her: bilder) til server ved hjelp av en protokoll implementert for anledningen. Server sjekker om mottatt fil matcher en lokal fil og skriver resultatet til en output-fil.
Protokollen er bygget på UDP og skal håndtere pakketap (med Go-Back-N). Har også Flow Control.
Filstørrelsene er slik at de kan rommes av én UDP-frame.

# Bruk

Klienten avslutter hvis kommandolinje-argumentet "filnavn" ikke er et gyldig filnavn.
Internt, når den leser filene oppgitt i filnavnlisten ignorerer den ugyldige filnavn, istedenfor å avbryte kjøring.

Antar at tapsprosent-argumentet er en int.
Gjør konvertering internt fra int til float.

Filen som server skriver til må være en eksisterende fil. Denne blir overskrevet av programmet.
Programmet avslutter hvis utskriftsfilen ikke finnes.

Server-programmet godtar en opsjonal taps-sannsynlighet (int), i tillegg til et debug-parameter (debug-utskrift under kjøring).


## Eksempel – server

`./server <portnum> <directory w/imgs> <output filename> [<loss probability (int) 0-100>] [-d]`

`./server 1337 img_set resultat.txt`   -> tapssannsynlighet settes til 0%

`./server 1337 img_set resultat.txt 30` -> tapssannsynlighet settes til 30%

`./server 1337 img_set resultat.txt -d` -> debug-mode med 0% tapssannsynlighet

`./server 1337 img_set resultat.txt 20 -d` -> debug-mode med 20% tapssannsynlighet


## Eksempel – klient

`./client <hostname/address> <portnum> <file with paths> <loss probability (int) 0-100> [-d]`

`./client 127.0.0.1 1337 list_of_filenames.txt 10` -> tapssannsynlighet settes til 10%

`./client 127.0.0.1 1337 list_of_filenames.txt 10 -d` -> debug-mode med 20% tapssannsynlighet


# Bemerkninger
Fungerer ikke med ipv6-adresser for øyeblikket.


# Todo
Skrive "compare"-funksjon som erstatter handouts.
