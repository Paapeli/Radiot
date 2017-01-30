#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

#define CH_AMOUNT 4
#define uS 2 //skaalaus countterin arvost ms:hin

#define PPM_LENGTH_REG OCR1A



void init_system(void);
void read_pot(uint8_t pot_number);


typedef enum {MID_PULSE, CH_PULSE, END_PULSE} ppm_state_type;
volatile ppm_state_type ppm_state = MID_PULSE;

volatile uint8_t current_chan = 0;
volatile uint16_t total_pulse_length = 0;



volatile uint16_t servo_pulse_table[CH_AMOUNT] =  
{
  2000, //kaasu
  1500, //siivekkeet
  1500, //korkkari
  2000  //sivari
};


int main(void) {
   init_system();
   while (1) {
   }
}


void init_system() {
   cli();
   // Alustetaan PPM-pinni ulostuloks
   DDRB |= (1<<PB1); // (DDR_OC1A~PB1)
   
   // Debuggausledi päälle
   DDRB |= (1<<PB5);
   PORTB |= (1<<PB5);

   // ADC:n alustus
   // Valitaan Vcc jännitereferenssiksi
   ADMUX |= (1<<REFS0);
   // ADLAR 1:ksi, jotta voidaan (alkuvaiheessa) lukea
   // helposti vaan 8 bittiä
   ADMUX |= (1<<ADLAR);
   // Esijakajaksi 128, jolloin muuntimen kello 125kHZ
   ADCSRA |= (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
   // Asetetaan keskeytys, kun muunnos on valmis
   ADCSRA |= (1<<ADIE);
   // Enabloidaan AD-muunnos
   ADCSRA |= (1<<ADEN);

   
   // Timerin alustus
   
   // Nollataan 16-bit timerin kontrollirekkarit
   TCCR1A = 0;
   TCCR1B = 0;
   // Alustetaan compare match-rekkariin joku arvo
   PPM_LENGTH_REG = 10000*uS; //1ms
   // Esijakajan kannattaa olla 8, jolloin pienin 
   // steppi on 0,5us ja maximiarvo ~32,7ms 
   TCCR1B |= (1 << CS11); 
   // CTC-moodi päälle (Clear Timer on Compare)
   TCCR1B |= (1 << WGM12);
   // Pinnin pitää toglata heti, kun tulee compare match
   TCCR1A |= (1 << COM1A0);
   // Enabloidaan timer compare interrupti
   TIMSK1 |= (1 << OCIE1A);
   sei();
}

void read_pot(uint8_t pot_number) {
   if (pot_number == 0) {
   ADMUX &= ~(1 << MUX0);
   ADMUX &= ~(1 << MUX1);
   ADMUX &= ~(1 << MUX2);
   ADMUX &= ~(1 << MUX3);
   ADCSRA |= (1<<ADSC);
   } else if (pot_number == 1) {
   ADMUX |= (1 << MUX0);
   ADMUX &= ~(1 << MUX1);
   ADMUX &= ~(1 << MUX2);
   ADMUX &= ~(1 << MUX3);
   ADCSRA |= (1<<ADSC);
   } else if (pot_number == 2) {
   servo_pulse_table[2] = 1500;
   } else {
   servo_pulse_table[3] = 1500;
   }
}

ISR(ADC_vect){
    uint16_t servo_pulse;
    uint16_t conv_result = (uint16_t)(ADCH<<2);
    servo_pulse = 1000 + conv_result;
    //servo_pulse = 1000 + (((uint16_t)ADCH)*1000/256);
    servo_pulse_table[current_chan] = servo_pulse;
}

ISR(TIMER1_COMPA_vect){
   // Lähetettävää signaalia hoidetaan tilakoneella
   if(ppm_state == MID_PULSE) {
      PPM_LENGTH_REG = 300*uS;
      total_pulse_length += 300;
      if (current_chan <= CH_AMOUNT-1) {
	ppm_state = CH_PULSE;
      } else {
         ppm_state = END_PULSE;
      }
   } else if (ppm_state == CH_PULSE) {
      PPM_LENGTH_REG = servo_pulse_table[current_chan]*uS;
      total_pulse_length += servo_pulse_table[current_chan];
      current_chan += 1;
      read_pot(current_chan);
      ppm_state = MID_PULSE;
   } else { //ppm_state == END_PULSE
      PPM_LENGTH_REG = (22500 - total_pulse_length)*uS;
      total_pulse_length = 0;
      read_pot(0);
      current_chan = 0;
      ppm_state = MID_PULSE;
   }
}

/*
Ideoita:
- Jos PPM-pitää invertoida niin tarvii tehä vähän monimutkasempi, mutta eiköhän moduuli syö tällästäkin...

*/
