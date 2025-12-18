**Logs.rar**

1. I starten af loggene (d. 17.) var der en del "bottleneck" i "pipeline structuren". Da tråden ventede ofte samt var der nogle hukommelseskopiering fejl, som skabte unødvendig latency. Dette resulterede i de høje, ustabile AI tider.

2. For at modvirke dette, implementerede jeg en række tidlige 'validity checks' og bounding box filtre. Ved hurtigt at frasortere ugyldige eller irrelevante frames dette gjorde så jeg kunne frigøre nogle ressourcer. Dette ses i graferne som perioder, hvor gennemsnits FPS begynder at stige, da der var mindre spildt arbejde.

3. Jeg tilføjede  Prediction (baseret på bevægelse). Dette gjorde det muligt for systemet at gætte positioner imellem AI scanninger, hvilket effektivt hjalp rendering hastigheden fra detektions hastigheden.

4. Den 18. december kl. 11:54 fixede jeg det hvor alle memory leaks jeg kunne finde var lukke. Resultatet var reelt ret stabilt pga. en flad kurve uden spikes, hvilket beviser, at softwaren nu er fuldt optimeret til at køre på min dårlige pc
<img width="1198" height="599" alt="image" src="https://github.com/user-attachments/assets/cdec8749-cd4f-46a9-a64b-a2f6aaace13c" />
