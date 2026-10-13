These binary fixtures were written by unmodified firmware at commit
`590e7cd2af42b7b35efb8e6aa01f72ba57155185`, using its real `proj_pack`,
`bank_pack` and `template_save` paths. They freeze the pre-FX FUN16, FBKK
and TPLF formats independently of the new decoder.

Track 1 has FM6 engine parameter E0 = 17 and an obsolete FX field set to 127.
Its first step is C/E/G with velocity 100. Another pattern bank has note 72.
The recording has onset 55, duration 217, owner 11 and pitch offset 3.
Motion addresses the old E0 ID, 96. The new decoder must move E0 to 108,
preserve this material and initialise all new FX slots dry.

`fun17.bin`, `fbkk17.bin` and `tplg.bin` were written by unmodified firmware at
`1673b24` through those same save paths. They additionally carry ECHO in the
original insert, with MIX 91, TIME 83 and REPEAT 77; engine motion uses E0 ID
100. Loading them must retain that insert in slot 1, leave slots 2 and 3 off,
and preserve the musical data above.

`fun18.bin`, `fbkk18.bin`, and `tplh.bin` are frozen synthetic three-insert records from the 116-track-parameter, 27-global layout. They contain Phaser/Echo/Reverb in slots 1/2/3 and a C4 note. They verify migration to SEND/RETURN without altering the insert chain; older records have no sends or shared effects.
