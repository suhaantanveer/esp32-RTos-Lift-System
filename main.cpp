#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

typedef enum{
  Ground_Floor=0,
  First_Floor=1,
  Second_Floor=2,
  Third_Floor=3,
  Fourth_floor=4,
  Fifth_floor=5,
  NONE = 6
}Floor;

typedef enum{
  IDLE,
  MOVING_UP,
  MOVING_DOWN,
  DOOR_OPEN,
  EMERGENCY
}Lift;

Lift lift_state = IDLE;
Lift previous_state = IDLE;

const int green = 2;
const int red = 4;
const int button1 = 18;
const int buzzer = 5;
const int button2 = 19; 
const int Max_size = 10;
Floor queue[Max_size];
int size = 3;
volatile bool emergency = false;



Floor Lift_floor = Ground_Floor;
Floor required_floor = Ground_Floor;


void State_machine_lift(void *pvParameters);
void State_checker(void *pvParameters);
void interrupting();

void setup() {
  Serial.begin(115200);
  pinMode(green,OUTPUT);
  pinMode(red,OUTPUT);
  pinMode(button1,INPUT_PULLUP);
  pinMode(button2,INPUT_PULLUP);
  pinMode(buzzer,OUTPUT);
  Wire.begin(21, 22);


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAILED");
    while (1);
  }
  Serial.println("OLED OK");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  attachInterrupt(digitalPinToInterrupt(button1), interrupting, FALLING);

  xTaskCreate(
    State_machine_lift,
    "Lift System",
    2048,
    NULL,
    1,
    NULL
  );

  xTaskCreate(
    State_checker,
    "Lift State",
    2048,
    NULL,
    2,
    NULL
  );  

}

String Lift_to_string(Lift lift){
  if(lift == IDLE) return "IDLE";
  if(lift == MOVING_UP) return "MOVING UP";
  if(lift == MOVING_DOWN) return "MOVING DOWN";
  if(lift == DOOR_OPEN) return "DOOR OPEN";
  if(lift == EMERGENCY) return "EMERGENCY";
  else{
    return "IDLE";
  }
}

String Floor_to_string(Floor floor){
  if(floor == Ground_Floor) return "G";
  if(floor == First_Floor) return "1st";
  if(floor == Second_Floor) return "2nd";
  if(floor == Third_Floor) return "3rd";
  if(floor == Fourth_floor) return "4th";
  if(floor == Fifth_floor) return "5th";
  else{
    return "IDLE";
  }
}

void check_state(){
  int buttonstate1 = digitalRead(button1);
  int buttonstate2 = digitalRead(button2);
  if(buttonstate1 == LOW){
    lift_state = MOVING_UP;
    previous_state = IDLE;
    while(digitalRead(button1) == LOW);
  }
  if(buttonstate2 == LOW){
    lift_state = MOVING_DOWN;
    previous_state = IDLE;
    while(digitalRead(button2) == LOW);
  }

}

void IRAM_ATTR interrupting(){
  emergency = true;
}

void lift_moving(){
  digitalWrite(red,HIGH);
  digitalWrite(green,LOW);
  vTaskDelay(pdMS_TO_TICKS(4000));
}

Floor String_to_floor(String string){
  if (string == "G") return Ground_Floor;
  else if (string == "1") return First_Floor;
  else if (string == "2") return Second_Floor;
  else if (string == "3") return Third_Floor;
  else if (string == "4") return Fourth_floor;
  else if (string == "5") return Fifth_floor;
  else{
    return Ground_Floor;
  }
}


void queue_amend(Floor arr[], Floor floor){
  arr[size] = floor;
  size++;
}

void queue_remove(Floor arr[], Floor floor){
  for(int i = 0; i< size; i++){
    if(arr[i] == floor){
      for(int j = i; j<size-1; j++){
        arr[j] = arr[j+1];
      }
      size--;
      break;
    }
  }
}

void display_whole(){
  display.println(Lift_to_string(lift_state));
  display.print("Current Floor: ");
  display.println(Floor_to_string(Lift_floor));
  display.display();
}

void State_checker(void *pvParameters){
  while(true){
    if(Serial.available()){
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      queue_amend(queue,String_to_floor(cmd));
    }

    if (!emergency){

      if (lift_state==IDLE && previous_state == IDLE){
        Floor smallest = queue[0];
        int index=0;
        int diff= abs(Lift_floor - smallest);
        for(int i = 0; i<size; i++){
          if(abs(Lift_floor - queue[i]) < diff){
            smallest = queue[i];
            index=i;
            diff= abs(Lift_floor - queue[i]);
          }

        }  
          
        required_floor = smallest;
        queue_remove(queue,smallest);
      }

      else if (lift_state ==IDLE && previous_state == MOVING_UP){
        Floor smallest = queue[0];
        int index=0;
        int diff= abs(Lift_floor - smallest);
        for(int i = 0; i<size; i++){
          if(abs(Lift_floor - queue[i]) < diff && queue[i] > Lift_floor){
            smallest = queue[i];
            index=i;
            diff= abs(Lift_floor - queue[i]);
          }

        }  
          
        required_floor = smallest;
        queue_remove(queue,smallest);
        
      }

      else if (lift_state==IDLE && previous_state == MOVING_DOWN){
        Floor smallest = queue[0];
        int index=0;
        int diff= abs(Lift_floor - smallest);
        for(int i = 0; i<size; i++){
          if(abs(Lift_floor - queue[i]) < diff && queue[i] < Lift_floor){
            smallest = queue[i];
            index=i;
            diff= abs(Lift_floor - queue[i]);
          }

        }  
        required_floor = smallest;
        queue_remove(queue,smallest);
      }
    }
    else{
      required_floor = Ground_Floor;
      size = 0;
      lift_state = MOVING_DOWN;
      emergency = false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }  
}

void door_open(){
  digitalWrite(green,LOW);
  digitalWrite(red,LOW);
  digitalWrite(buzzer,HIGH);
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void lift_idle(){
  digitalWrite(green,HIGH);
  digitalWrite(red,LOW);
  vTaskDelay(pdMS_TO_TICKS(100));
}

void State_machine_lift(void *pvParameters) {

  while(true){
    display.clearDisplay(); 
    display.setCursor(0, 10); 

    if(required_floor > Lift_floor && (previous_state == IDLE || previous_state == MOVING_UP)){
      lift_state = MOVING_UP;
    }
    else if(required_floor < Lift_floor && (previous_state == IDLE || previous_state == MOVING_DOWN)){
      lift_state = MOVING_DOWN;
    }
    else if(required_floor == Lift_floor && previous_state!= IDLE){
      previous_state = IDLE;
      lift_state = DOOR_OPEN;
    }
    else{
      lift_state = IDLE;
    }


    switch(lift_state){
      case IDLE:
        display_whole();
        lift_idle();
        break;
      case MOVING_UP:
        display_whole();        
        lift_moving();
        Lift_floor = (Floor)(Lift_floor + 1);
        previous_state = lift_state;
        lift_state = (Lift_floor == required_floor)? DOOR_OPEN:MOVING_UP;
        break;
      case DOOR_OPEN:
        display_whole();
        door_open();
        digitalWrite(buzzer,LOW);
        lift_state = IDLE;
        break;
      case MOVING_DOWN:
        display_whole();
        lift_moving();
        Lift_floor = (Floor)(Lift_floor - 1);
        previous_state = lift_state;
        lift_state = (Lift_floor == required_floor)? DOOR_OPEN:MOVING_DOWN;
        break;


    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }  
}

void loop(){

}

// Things i've to do, make sure when the lift is running or paused