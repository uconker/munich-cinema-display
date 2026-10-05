# Builds mock_api.json in the same shape as Kinoheld's programShows response, from real rows seen on 2026-10-06.
import json
R = """Mathäser Filmpalast|13:00|Coyote vs. ACME|101|D-BOX|6
Mathäser Filmpalast|13:15|Das gewisse Etwas|105||6
ARRI-Kino|13:30|CORPUS DELICTI |103||
ARRI-Kino|13:45|Vaterland|82||0
Mathäser Filmpalast|13:45|Spider-Man: Brand New Day|146||12
Mathäser Filmpalast|13:45|Emil und die Detektive|91||6
ABC KINO MÜNCHEN|14:00|VATERLAND |81||
LEOPOLD KINO MÜNCHEN|14:00|Bibi Blocksberg - Die total verhexte Zeitreise|90|Kinder & Familienkino im Leopold|0
Mathäser Filmpalast|14:00|Verity - Dunkle Geheimnisse|114|4K|12
Museum Lichtspiele München|14:00|Emil und die Detektive|91||6
Museum Lichtspiele München|14:00|Practical Magic 2 - Zauberhafte Schwestern|130|OV|12
Rio Filmpalast München|14:00|Shaun das Schaf – Spuk im Kürbisfeld|80|Familie|0
Mathäser Filmpalast|14:15|Digger|129||12
Mathäser Filmpalast|14:15|Steckerlfischfiasko|99||12
CinemaxX München|14:30|Verity - Dunkle Geheimnisse|114||12
CinemaxX München|14:30|Paw Patrol: Der Dino Film|83||0
City Kino|14:30|Das geträumte Abenteuer|164||12
Museum Lichtspiele München|14:30|Shaun das Schaf – Spuk im Kürbisfeld|80|OV|0
CinemaxX München|14:40|Digger|129||12
Cineplex Neufahrn bei Freising|15:40|Shaun das Schaf – Spuk im Kürbisfeld|80||0
Neues Maxim Kino München|15:40|Bagger Drama|96||12
Neues Maxim München (alt)|15:40|Bagger Drama|96||12
Monopol Kino München|15:50|Primetime|107|subtitled OV|
Monopol Kino München|16:00|Finding Emily|111|subtitled OV|6
Theatiner Film|16:00|Albrecht Weinberg - Es ist immer in meinem Kopf|90||12
Gloria-Palast|16:30|Steckerlfischfiasko|99||12
Mathäser Filmpalast|16:30|Verity - Dunkle Geheimnisse|114|4K|Atmos|D-BOX|12
Mathäser Filmpalast|16:30|Die Odyssee|172|4K|D-BOX|12
Studio Isabella München|16:30|ÜBER UNTERBIBERGER – VOM WOID IN DIE WELT|92||0
Royal Filmpalast München|16:45|Coyote vs. ACME|101||6
CinemaxX München|17:00|Verity - Dunkle Geheimnisse|114||12
Mathäser Filmpalast|17:00|Spider-Man: Brand New Day|146|4K|12
Cadillac Veranda Kino München|17:45|Steckerlfischfiasko|99||12
Werkstattkino|17:45|OF MUD AND BLOOD: KONGO – IM RAUSCH DER HOFFNUNG||| 
Werkstattkino|17:45|Of Mud and Blood|90||12
Kino Haar|17:45|Bibi Blocksberg - Die total verhexte Zeitreise|90||0
Museum Lichtspiele München|17:55|Spider-Man: Brand New Day|146|OV|12
astor@CINEMA LOUNGE|18:00|Spa Weekend|100||12
Kulturverein Olympiadorf|18:00|Qigong||| 
Monopol Kino München|18:00|Digger|129|subtitled OV|16
Museum Lichtspiele München|18:00|Digger|129|OV|16
Arena Filmtheater München|18:15|Digger|129|subtitled OV|16
ARRI-Kino|18:15|Digger|129||12
LEOPOLD KINO MÜNCHEN|19:00|Live-Kino-Podcast "Never Enough" mit Doris Dörrie|160|Event im Leopold|
Mathäser Filmpalast|19:30|Verity - Dunkle Geheimnisse|114|4K|12
Mathäser Filmpalast|19:30|CORPUS DELICTI |103||
Mathäser Filmpalast|19:45|Digger|129||12
CinemaxX München|20:00|Spider-Man: Brand New Day|146||12
Mathäser Filmpalast|20:15|Pans Labyrinth |112||16
Monopol Kino München|20:30|Pans Labyrinth |112|subtitled OV|Best of Cinema|16
Theatiner Film|20:30|BAGGER DRAMA – Max-Ophüls-Preis Beste Regie!|||
Filmmuseum|21:00|Die Schwindler|112||12
Mathäser Filmpalast|21:30|Insidious: Out of the Further|106||16
Mathäser Filmpalast|22:15|Mutiny|95||18
Mathäser Filmpalast|22:45|Resident Evil|99||16
Mathäser Filmpalast|22:45|Resident Evil|99||16""".split("\n")
rows=[]
for line in R:
    p=[x.strip() if i!=2 else x for i,x in enumerate(line.split("|"))]
    cin,t,title=p[0],p[1],p[2]; dur=p[3]; age=p[-1]; flags=[x for x in p[4:-1] if x]
    rows.append((cin,t,title,int(dur) if dur.isdigit() else None,flags,int(age) if age.isdigit() else None))
# add the same program again for the next day (shifted) so day 1 is populated, plus a 00:30 show
data=[]
for day in ("2026-10-06","2026-10-07"):
    for i,(cin,t,title,dur,flags,age) in enumerate(rows):
        city="München" if cin not in ("Cineplex Neufahrn bei Freising","Kino Haar") else cin.split()[-1]
        data.append({"id":str(len(data)),"name":title,"beginning":f"{day}T{t}:00+02:00","flags":[{"name":f} for f in flags],
          "cinema":{"id":"1","name":cin,"city":{"name":city}},
          "movie":{"id":"1","title":title,"duration":dur,"contentRating":{"minimumAge":age} if age is not None else None}})
data.append({"id":"x","name":"Late","beginning":"2026-10-07T00:30:00+02:00","flags":[],"cinema":{"id":"1","name":"Mathäser Filmpalast","city":{"name":"München"}},"movie":{"id":"9","title":"Midnight Movie","duration":90,"contentRating":{"minimumAge":16}}})
json.dump({"data":{"programShows":{"data":data,"paginatorInfo":{"hasMorePages":False,"currentPage":1}}}},open("mock_api.json","w"),ensure_ascii=False)
print(len(data),"mock shows")
