void crudeParse(char str[], unsigned char * ret, unsigned char * index);

unsigned char dayStartTime;
unsigned char nightStartTime;
unsigned char targetTemp;
unsigned char targetRH;
unsigned char index;
char HTTP_req[100] = "GET /?dayStart=8&nightStart=15&targetTemp=85&targetRh=65 HTTP/1.10L";

void setup() {
  Serial.begin(9600);
}

void loop()
{
  crudeParse(HTTP_req, &dayStartTime, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);
  crudeParse(HTTP_req, &nightStartTime, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);
  crudeParse(HTTP_req, &targetTemp, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);
  crudeParse(HTTP_req, &targetRH, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);

  Serial.println("dayStartTime: ");
  Serial.println((int)dayStartTime);
  Serial.println("nightStartTime: ");  
  Serial.println((int)nightStartTime);
  Serial.println("targetTemp: ");
  Serial.println((int)targetTemp);
  Serial.println("targetRH: ");
  Serial.println((int)targetRH);

  //Just chill
  while (1) {
  }
}

void crudeParse(char str[], unsigned char * ret, unsigned char * index) {

  unsigned char i = 0;
  unsigned char num = 0;
  while (str[i]) {
    if ('=' == str[i]) {
      num = num*10 + (str[++i] - '0');
      i++;

      while (!('\0' == str[i] || '&' == str[i] || ' ' == str[i])) {
        num = num*10 + (str[i]-'0');
        i++;
      }
      *index = i;

      break;
    }
    i++;
  }

  *ret = num;
}




