#include <Arduino.h>

// Define the notes
#define NOTE_A4  440 
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978
#define NOTE_C4  262   //Defining note frequency
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440

int melody[] = {

  // note susu murni
  NOTE_C5, NOTE_C5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_C5, 0, 0,

  // // note warkoDKI
  // NOTE_C5, NOTE_C5, NOTE_C5, NOTE_B4, NOTE_D5, NOTE_B4, 0,
  // NOTE_D5, NOTE_D5, NOTE_D5, NOTE_C5, NOTE_E5, NOTE_C5, 0,
  // NOTE_C5, NOTE_C5, NOTE_C5, NOTE_B4, NOTE_D5, NOTE_B4,
  // NOTE_C5, NOTE_C5, NOTE_B4, NOTE_B4, NOTE_A4,
  // NOTE_E6, NOTE_E6, NOTE_E6, NOTE_D6, NOTE_F6, NOTE_D6, 0, // 3-3-3-2-4--2
  // NOTE_D6, NOTE_D6, NOTE_D6, NOTE_C6, NOTE_E6, NOTE_C6, 0, // 2-2-2-1-3--1
  // NOTE_C5, NOTE_C5, NOTE_C5, NOTE_B5, NOTE_D5, NOTE_B5, // 1-1-1-7-2-7
  // NOTE_C5, NOTE_C5, NOTE_B5, NOTE_B5, NOTE_A5           // 1-1-7-7-6--

};

int noteDurations[] = {

  4, 4, 4, 2, 4 , 2, 2 , 1, 0, 0,

  // 4, 4, 4, 4, 3, 4, 0,
  // 8, 4, 4, 4, 3, 4, 0,
  // 8, 4, 4, 4, 3, 4,
  // 4, 4, 3, 3, 2,
  // 8, 4, 4, 4, 3, 4, 0,
  // 8, 4, 4, 4, 3, 4, 0,
  // 8, 4, 4, 4, 3, 4,
  // 4, 4, 3, 3, 2,
};

void RingTone(){
  int totalNotes = sizeof(melody) / sizeof(melody[0]);
  for (int thisNote = 0; thisNote < totalNotes; thisNote++) {
    if (melody[thisNote] == 0) {
      delay(100);
    }else{
      int noteDuration = 1000 / noteDurations[thisNote];
      tone(18, melody[thisNote], noteDuration);
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
    }
  }
  // Add a delay between repeats of the melody
  delay(1000);
}