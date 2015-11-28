//*************** PRESENTATION DU PROGRAMME ***************

// -------- Que fait ce programme ? ---------
/* Crée un serveur domotique, qui permet de contrôler une lampe avec 3 alarmes.
 	Le bouton d'allumage/extinction est intelligent et s'adapte en fonction de la situation
 Utilisation d'une struct pour les alarmes, et début de découpage en fonctions
 Implémentation de la structure de Sorties
 Version 2.9
 */

// --- Fonctionnalités utilisées ---

// Utilise la connexion série vers le PC
// Utilise le module Ethernet Arduino
// en mode serveur HTTP

// utilise la librairie Flash de stockage en mémoire programme FLASH

//****************** Entête déclarative ****************

// --- Inclusion des librairies ---
#include <Time.h>
#include <TimeAlarms.h>
//#include <Wire.h>  
//#include <DS1307RTC.h>
#include <EEPROM.h>

//-- librairies utilisées pour le module Ethernet
#include <SPI.h>
#include <Ethernet.h>
#include <Server.h>
#include <Client.h>

// librairie pour stockage en mémoire flash programme
#include <Flash.h>
// attention : l'utilisation de F("chaine") necessite modification
// du fichier print.h
// voir : www.mon-club-elec.fr/pmwiki_reference_arduino/pmwiki.php?n=Main.LibrairieFlashProgramme

//--- déclaration du tableau d'adresse MAC ---
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // valeur arbitraire

//---- tableau de l'adresse IP de la carte Arduino
byte ip[] = { 
  192,168,1,18 }; // le PC a pour IP : 192.168.1.18

//----- tableau de l'adresse de la passerelle ---
byte passerelle[] = { 
  192, 168, 1, 1 }; // l'adresse du PC de connexion ou de la Box

//----- tableau du masque de sous réseau
byte masque[] = { 
  255, 255, 255, 0 }; // idem masque sous-réseau du PC : 255.255.255.0

// variables globales utiles
String chaineRecue=""; // déclare un string vide global
int page = 1;    // Variable permettant d'indiquer la page demandée par la requête HTTP

/************* Structures ******************/

#define NB_MAX_ALARMES 3
#define NB_SORTIES 1

struct Alarme_s {
  AlarmID_t idAlarme; // Identifiant de l'alarme
  int active;         // Alarme activée ? 1 : oui ; 0 : non
  OnTick_t action;    // Type d'action de l'alarme. 1 : eclairer ; 2 : eteindre
  int typeAlarme;      // Type de l'alarme : 1 pour une alarme se répétant tous les jours ; (0 pour régler l'heure)
};

struct Sorties_s {
  int pin;
  int activee;
  int etat;
  int nbAlarmes;
  Alarme_s alarmes[NB_MAX_ALARMES];
};
Sorties_s Sorties[NB_SORTIES];

Sorties_s Lampe;

struct Horaire_s {
  int heure;
  int minutes;
  int secondes;
};

/****** Defines EEPROM ********/
//  | EEPROM_DEBUT_SORTIES | EEPROM_INTRO_SORTIE | EEPROM_TAILLE_ALARME | EEPROM_TAILLE_ALARME | ... | EEPROM_INTRO_SORTIE | EEPROM_T
// ILLE_ALARME | EEPROM_TAILLE_ALARME | ... |

#define EEPROM_DEBUT_SORTIES 0
#define EEPROM_INTRO_SORTIE 2
#define EEPROM_TAILLE_ALARME 6
#define EEPROM_TAILLE_SORTIE (EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * NB_MAX_ALARMES))
#define EEPROM_TAILLE_TOUTES_SORTIES (EEPROM_TAILLE_SORTIE * NB_SORTIES)

// --- Déclaration des objets utiles pour les fonctionnalités utilisées ---

//--- création de l'objet serveur ----
EthernetServer serveurHTTP(80); // crée un objet serveur utilisant le port 80 = port HTTP

int initialiser_alarmes(int sortie_a_initialiser) // On initialise les alarmes d'une sortie, avec les données contenues dans l'Eeprom. Renvoie le nombre d'alarmes de la sortie
{
  int index_eeprom_sortie = EEPROM_DEBUT_SORTIES + (sortie_a_initialiser * EEPROM_TAILLE_SORTIE); // La première case de la sortie
  int index_alarme;

  for(int i = 0 ; i < Lampe.nbAlarmes ; i++)
  { 
    index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * i);

    Lampe.alarmes[i].active = EEPROM.read(index_alarme);  
    Lampe.alarmes[i].typeAlarme = EEPROM.read(index_alarme + 2);

    if(EEPROM.read(index_alarme + 1) == 1)
    {
      Lampe.alarmes[i].action = MorningAlarm;
    }
    if(EEPROM.read(index_alarme + 1) == 2)
    {
      Lampe.alarmes[i].action = EveningAlarm;
    }
    Lampe.alarmes[i].idAlarme = Alarm.alarmRepeat(AlarmHMS(EEPROM.read(index_alarme + 3), EEPROM.read(index_alarme + 4), EEPROM.read(index_alarme + 5)), Lampe.alarmes[i].action);
  }
}

void initialiser_serveur(byte mac[], byte ip[], byte passerelle[], byte masque[])
{
  //---- initialise la connexion Ethernet avec l'adresse MAC, l'adresse IP et le masque
  Ethernet.begin(mac, ip, passerelle, masque);

  //---- initialise le serveur HTTP----
  serveurHTTP.begin(); //---- initialise le serveur HTTP
}

void initialiserSorties()
{
  int index_eeprom_sortie; // La première case de la sortie
  
  Lampe.pin = 2; 

    index_eeprom_sortie = EEPROM_DEBUT_SORTIES + (0 * EEPROM_TAILLE_SORTIE);
    
    Lampe.etat = 0;
    Lampe.activee = EEPROM.read(index_eeprom_sortie);
    Lampe.nbAlarmes = EEPROM.read(index_eeprom_sortie + 1);
    
    pinMode (Lampe.pin,OUTPUT);
    initialiser_alarmes(0);

}

void setup()   
{
  setTime(0,0,0,11,2,13); //On règle l'heure à 0
  //setSyncProvider(RTC.get); // Synchronisation de l'heure ?

  Serial.begin(115200); // initialise connexion série à 115200 bauds

  initialiserSorties(); // On ilitialise les sorties et leurs alarmes associées

  Serial.println(F("Init Sorties OK"));

  initialiser_serveur(mac, ip, passerelle, masque); // Initialisation du serveur
  
  Serial.println(F("Init serveur OK"));
} 

void reception_requete(EthernetClient client)
{
  chaineRecue=""; // vide le String de reception
  int comptChar=0; // compteur de caractères en réception à 0  

    Serial.println(F("------------ Reception de la requete Client ------------")); // affiche le String de la requete
  Serial.println (F(""));

  while (client.available()) { // tant que des octets sont disponibles en lecture

      char c = client.read(); // lit l'octet suivant reçu du client (pour vider le buffer au fur à mesure !)
    comptChar=comptChar+1; // incrémente le compteur de caractère reçus

    //--- on ne mémorise que les n premiers caractères de la requete reçue
    //--- afin de ne pas surcharger la RAM et car cela suffit pour l'analyse de la requete
    if (comptChar<=100) chaineRecue=chaineRecue+c; // On ne prend que 100 caractères

    Serial.print(c); // message debug - affiche la requete entiere reçue
  }

  // si plus de caractères disponibles = la requete client terminée
  Serial.println(F("Reception requete client terminee... ")); // message debug
  Serial.println(F("")); // message debug
}

void loop(){ // debut de la fonction loop()

  Serial.println(F("Loop"));

  Alarm.delay(0); // Pour vérifier si des alarmes doivent être activées
  
  Serial.println(F("Alarm OK"));

  EthernetClient client = serveurHTTP.available(); // si un client est disponible, création de l'objet client correspondant
  
  Serial.println(F("Serveur OK"));

  if (client) { // si l'objet client n'est pas vide = si le client existe

    Serial.println(F("------------ Connexion Client ------------")); // message début de connexion
    Serial.println (F(""));
    Serial.println(F("Detection client...")); // message debug

    if (client.connected()) { // si le client est connecté

      Serial.println(F("Connexion avec client OK ... ")); // message debug
      Serial.println(F("")); // message debug

      //////////////// Reception de la requete envoyée par le client //////////////////

      reception_requete(client);

      /////////////////// Analyse de la requete reçue //////////////////////

      analyser_requete();

      //////////// ENVOI DE LA REPONSE DU SERVEUR ///////////////

      Serial.println(F("------------ Envoi de la reponse au client ------------")); // affiche le String de la requete
      Serial.println (F(""));

      envoyer_reponse(client); // Envoi de la page HTML requise au client     
    }

    Alarm.delay(1);
    // on donne au navigateur le temps de recevoir les données

    // fermeture de la connexion avec le client après envoi réponse
    client.stop();

    Serial.println(F("Fin existence client")); // message debug
    Serial.println(F("")); // message debug

  } //---- fin if client existe ----
} 

void analyser_requete()
{
  int index_eeprom_sortie = 0; // La première case de la sortie
  int index_alarme;

  Serial.println(F("------------ Analyse de la requete recue ------------")); // affiche le String de la requete
  Serial.println (F(""));

  Serial.print (F("Chaine prise en compte pour analyse : "));
  Serial.println(chaineRecue); // affiche le String de la requete pris en compte pour analyse
  Serial.println ("");

  // variables pour analyse de la requete reçue
  String chaineAnalyse=""; // crée un string utilisé pour analyser la requete
  String chaineAnalyseBuff="";
  int indexChaine=0; // variable index pour parcourir la chaine reçue

  //------------- analyse si présence données formulaire ---------

  //-- analyse la présence du ? => présent si appui sur bouton envoi coté client
  indexChaine=5; // analyse le 6ème caractère
  chaineAnalyse=chaineRecue.substring(indexChaine,indexChaine+1); // extrait le 6ème caractère
  Serial.print(F("Le 6eme caractere est : ")), Serial.println(chaineAnalyse);

  if (chaineAnalyse != " ") { // test si le 6ème caractère est un ?, pour voir si des données de formulaire sont disponibles


    // Swich (la requete trouvee)
    // Case toutes les requêtes possibles

    chaineAnalyse="EclairerLampe1";
    if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton allumage de la lampe
      digitalWrite(Lampe.pin, HIGH);
      Lampe.etat = 1;
    }

    chaineAnalyse="EteindreLampe1";       
    if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton extinction de la lampe
      digitalWrite(Lampe.pin, LOW);
      Lampe.etat = 0;
    }

    for(int j = 0; j < Lampe.nbAlarmes; j++){ //On teste si on demande de régler une des alarmes
      index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * j);

      chaineAnalyseBuff = "ReglageAlarme";
      chaineAnalyse = chaineAnalyseBuff + (j + 1); 
      if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton extinction de la lampe
        indexChaine = chaineRecue.indexOf(chaineAnalyse);

        if(Lampe.alarmes[j].active == 0){
          Lampe.alarmes[j].active = 1; 
          EEPROM.write(index_alarme, 1); // Active
        }
        reglageActionAlarme(Lampe.alarmes[j].idAlarme);
        reglageHoraireHttp(Lampe.alarmes[j].typeAlarme, Lampe.alarmes[j].idAlarme, indexChaine, Lampe.alarmes[j].action, 1); 

        page = 1;
      }
    }

    for(int j = 0; j < Lampe.nbAlarmes; j++){ 
      index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * j);

      chaineAnalyseBuff = "DesactAl";
      chaineAnalyse = chaineAnalyseBuff + (j + 1); 
      if(chaineRecue.indexOf(chaineAnalyse)!= -1){
        Lampe.alarmes[j].active = 0; 
        EEPROM.write(index_alarme, 0);
        Alarm.free(Lampe.alarmes[j].idAlarme);
        page = 1;
      }
    }

    for(int j = 0; j < Lampe.nbAlarmes; j++){ 
      index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * j);

      chaineAnalyseBuff = "ActivAl";
      chaineAnalyse = chaineAnalyseBuff + (j + 1); 
      if(chaineRecue.indexOf(chaineAnalyse)!= -1){
        Lampe.alarmes[j].active = 1; 
        EEPROM.write(index_alarme, 1);
        Lampe.alarmes[j].idAlarme = Alarm.alarmRepeat(AlarmHMS(EEPROM.read(index_alarme + 3), EEPROM.read(index_alarme + 4), EEPROM.read(index_alarme + 5)), Lampe.alarmes[j].action);
        page = 1;
      }
    }

    chaineAnalyse="ReglageHeure";  
    if (chaineRecue.indexOf(chaineAnalyse)!=-1){

      indexChaine = chaineRecue.indexOf(chaineAnalyse);

      reglageHoraireHttp(0, 0, indexChaine, NULL, 0);   

      page = 1;
    }

    chaineAnalyse="Accueil";       
    if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton extinction de la lampe
      page = 1;
    }

    chaineAnalyse="pageReglageDeHeure";       
    if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton extinction de la lampe
      page = 2;
    }

    for(int j = 0; j < Lampe.nbAlarmes; j++){ //Page de réglage des alarmes
      chaineAnalyseBuff = "pageReglageDeAlarme";
      chaineAnalyse = chaineAnalyseBuff + (j + 1);     
      if(chaineRecue.indexOf(chaineAnalyse)!= -1){//Bouton extinction de la lampe
        page = 3 + j;
      }
    } 

  } // fin if Chaineanalyse==?

  Serial.println(F("Analyse requete terminee...")); // message debug
  Serial.println(F("")); // message debug
}

void envoyer_reponse(EthernetClient client)
{
  // envoi d'une entete standard de réponse http
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close")); // indique au client que la connexion est fermée après réponse
  // à noter que la connexion est persistante par défaut coté client en l'absence de cette ligne          
  client.println(); // ligne blanche obligatoire après l'entete HTTP envoyée par le serveur

  // envoi du code HTML de la page

  //---- debut de la page HTML ---
  client.println(F("<html>"));

  // ---- Entete de la page HTML ----
  client.println(F("<head>"));

  client.println(F("<meta name=\"viewport\" content=\"width=device-width\"/>")); // Adaptation à la taille de l'écran
  //client.println("<meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"Content-Type\">");
  client.println(F("<title>Reveil Arduino</title>"));

  // balise meta pour réactualisation automatique de la page Web toutes les n secondes
  //client.println("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"1\">");

  client.println(F("</head>"));

  if(page == 1){
    pageAccueil(client); // Code HTML de la page d'accueil     
  }
  if(page == 2){
    pageReglageHeure(client);
  } 
  for(int j = 0; j < Lampe.nbAlarmes; j++){
    if(page == (3 + j)){
      pageReglageAlarme(j, client);
    } 
  }

  client.println(F("</body>"));

  //---- fin de la page HTML
  client.println(F("</html>"));
}

void reglageActionAlarme(AlarmID_t idAlarme){
  String chaineAnalyse="";

  int index_eeprom_sortie = 0; // La première case de la sortie
  int index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * idAlarme);

  chaineAnalyse="ALLUMER";

  if (chaineRecue.indexOf(chaineAnalyse)!=-1){
    EEPROM.write(index_alarme + 1, 1);
    Lampe.alarmes[idAlarme].action = MorningAlarm;
  }

  chaineAnalyse="ETEINDRE";

  if (chaineRecue.indexOf(chaineAnalyse)!=-1){
    EEPROM.write(index_alarme + 1, 2);
    Lampe.alarmes[idAlarme].action = EveningAlarm;
  }

}

int CharToChiffre(String chaineAnalysee, int indice)
{
  return (int)chaineAnalysee.charAt(indice) - 48;
}

int recupererNombreRequete(String requete, int index)
{
  int digit1;
  int digit2; 

  digit1 = CharToChiffre(requete ,index);
  if(requete.charAt(index + 1) != '&') //Si l'heure a deux digits
  {
    digit2 = CharToChiffre(requete ,index + 1); 
  }
  else
  {
    digit2 = digit1;
    digit1 = 0;           
  }
  return digit1*10 + digit2;
}

int chercherParametreChiffreDansRequete(String requete, String parametre)
{
  int indexChaine = requete.indexOf(parametre); // On se place au début du nom du paramètre

    if (indexChaine!=-1){ // Si le paramètre est présent dans la requête

    // On se place sur le premier caractère de l'information
    while(requete.charAt(indexChaine) != '=')
    {
      indexChaine++;
    }
    indexChaine++;

    if(requete.charAt(indexChaine) != '&') // Si il y a bien de l'information présente
    { 
      return recupererNombreRequete(requete, indexChaine); // On récupère l'information stockée
    }
    else
    { 
      return -1; // Renvoie -1 si il n'y a pas d'informations
    }
  }
}

void reglageHoraireHttp(int type, AlarmID_t idAlarme, int i, OnTick_t action, int miseEnMemoire){
  Horaire_s heure_reglee = {
    0, 0, 0        };

  int index_eeprom_sortie = 0; // La première case de la sortie
  int index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * idAlarme);

  heure_reglee.heure = chercherParametreChiffreDansRequete(chaineRecue, "HEURE");
  if(heure_reglee.heure == -1)
  {
    heure_reglee.heure = hour();
  }

  heure_reglee.minutes = chercherParametreChiffreDansRequete(chaineRecue, "MINUTE");
  if(heure_reglee.minutes == -1)
  {
    heure_reglee.minutes = minute();
  }

  heure_reglee.secondes = chercherParametreChiffreDansRequete(chaineRecue, "SECONDE");
  if(heure_reglee.secondes == -1)
  {
    heure_reglee.secondes = second(); 
  }

  if(miseEnMemoire == 1){ //Si on veut stocker dans l'Eeprom (Pour alarme)
    EEPROM.write(index_alarme + 3, heure_reglee.heure); //Eeprom alarme
    EEPROM.write(index_alarme + 4, heure_reglee.minutes);
    EEPROM.write(index_alarme + 5, heure_reglee.secondes);
  }

  if(type == 0){ //Si on règle l'heure
    setTime(heure_reglee.heure,heure_reglee.minutes,heure_reglee.secondes,5,2,13);

    //On actualise les alarmes
    for(int j = 0; j < Lampe.nbAlarmes; j++)
    {
      index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * j);

      if(Lampe.alarmes[j].active == 1){
        Alarm.free(Lampe.alarmes[j].idAlarme);
        Lampe.alarmes[j].idAlarme = Alarm.alarmRepeat(AlarmHMS(EEPROM.read(3 + index_alarme), EEPROM.read(4 + index_alarme), EEPROM.read(5 + index_alarme)), Lampe.alarmes[j].action);
      }
    }
  }
  else if(type == 1){ //Si on a une alarme se répétant tous les jours
    index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * idAlarme);

    Alarm.free(idAlarme);
    idAlarme = Alarm.alarmRepeat(AlarmHMS(EEPROM.read(3 + index_alarme), EEPROM.read(4 + index_alarme), EEPROM.read(5 + index_alarme)), action);
  }
}

void HTML_boutonFormulaire(EthernetClient client, String requete, String nomBouton)
{

  client.print(F("<form method=\"get\" action=\"")); 
  client.print(requete);
  client.print(F("\"><input type=\"submit\" value=\"")); 
  client.print(nomBouton);
  client.print(F("\" /></form>")); 
}

void pageReglageAlarme(int numeroAlarme, EthernetClient client){
  String requete_bouton;

  int index_eeprom_sortie = 0; // La première case de la sortie
  int index_alarme = index_eeprom_sortie + EEPROM_INTRO_SORTIE + (EEPROM_TAILLE_ALARME * numeroAlarme);

  if(Lampe.alarmes[numeroAlarme].active == 0){
    client.print(F("Alarme actuellement reglee pour "));
    client.print(EEPROM.read(3 + index_alarme));
    client.print(F(":"));
    client.print(EEPROM.read(4 + index_alarme));
    client.print(F(":"));
    client.println(EEPROM.read(5 + index_alarme));

    //Activation de l'alarme
    client.print(F("<form method=\"get\" action=\"ActivAl")); 
    client.print(numeroAlarme + 1);
    client.print(F("\">"));  
    client.print(F("<input type=\"submit\" value=\"Activer\" />"));          
    client.println(F("</form>"));
  }


  client.print(F("<form method=\"get\" action=\"ReglageAlarme"));   //super !
  client.print(numeroAlarme + 1);
  client.print(F("\">"));
  client.println(F("<fieldset><legend>"));  

  if(Lampe.alarmes[numeroAlarme].active == 1){
    if(Lampe.alarmes[numeroAlarme].action == MorningAlarm){
      client.print(F("La lampe s'allumera a : "));
    }
    else if(Lampe.alarmes[numeroAlarme].action == EveningAlarm){
      client.print(F("La lampe s'eteindra a : "));
    }
    afficherHoraireAlarme(client, numeroAlarme);
  }
  else{
    client.print(F("Reglage de l'alarme"));
  }
  client.println(F("</legend>")); 
  client.print(F("Heures"));       
  client.println(F("<input type=\"number\" name=\"HEURE\" />"));
  client.println(F("<br />"));
  client.print(F("Minutes"));    
  client.println(F("<input type=\"number\" name=\"MINUTE\" />"));
  client.println(F("<br />"));
  client.print(F("Secondes"));    
  client.println(F("<input type=\"number\" name=\"SECONDE\" />"));
  client.println(F("<br />"));

  client.println(F("<select name=\"Action\" id=\"Action\">"));
  client.println(F("<option value=\"ALLUMER\">Allumer</option>"));
  client.println(F("<option value=\"ETEINDRE\">Eteindre</option>"));
  client.println(F("</select>"));

  client.print(F("<input type=\"submit\" value=\"Regler\" />"));   
  client.println(F("</fieldset>"));       
  client.println(F("</form>"));

  if(Lampe.alarmes[numeroAlarme].active == 1){
    //Desactivation de l'alarme
    client.print(F("<form method=\"get\" action=\"DesactAl")); 
    client.print(numeroAlarme + 1);
    client.print(F("\">"));  
    client.print(F("<input type=\"submit\" value=\"Desactiver\" />"));          
    client.println(F("</form>"));
  }

  HTML_boutonFormulaire(client, "Accueil", "Retour");
}

void pageReglageHeure(EthernetClient client){
  client.print(F("Il est : "));
  afficherHeure(client);

  client.print(F("<form method=\"get\" action=\"ReglageHeure\">"));  
  client.println(F("<fieldset><legend>Reglage de l'heure</legend>"));  
  client.print(F("Heures"));    
  client.println(F("<input type=\"number\" name=\"HEURE\" />"));
  client.println(F("<br />"));
  client.print(F("Minutes"));    
  client.println(F("<input type=\"number\" name=\"MINUTE\" />"));
  client.println(F("<br />"));
  client.print(F("Secondes"));    
  client.println(F("<input type=\"number\" name=\"SECONDE\" />"));
  client.println(F("<br />"));
  client.print(F("<input type=\"submit\" value=\"Regler\" />")); 
  client.println(F("<INPUT type=\"text\" style=\"display:none\" name=\"vide\"<br>"));
  client.println(F("</fieldset>"));         
  client.println(F("</form>"));

  HTML_boutonFormulaire(client, "Accueil", "Retour");

  //Actualiser
  HTML_boutonFormulaire(client, "Actualiser", "Actualiser");
}

void pageAccueil(EthernetClient client){
  //----- Corps de la page HTML ---
  client.print(F("Il est : "));
  afficherHeure(client);

  // intègre une image - suppose connexion internet disponible
  //client.println("<CENTER> <img src=\"http://www.arduino.cc/mes_images/clipart/logo_arduino_150.gif\"> </CENTER>");
  //client.println(F("<CENTER> <img src=\"http://www.arduino.cc/mes_images/communs/led_rouge_5mm.gif\"> </CENTER>"));

  //Reglage de l'heure
  HTML_boutonFormulaire(client, "pageReglageDeHeure", "Regler l'heure");

  for(int j = 0; j < Lampe.nbAlarmes; j++){
    if(Lampe.alarmes[j].active == 1){
      if(Lampe.alarmes[j].action == MorningAlarm){
        client.print(F("La lampe s'allumera a : "));
      }
      else if(Lampe.alarmes[j].action == EveningAlarm){
        client.print(F("La lampe s'eteindra a : "));
      }
      afficherHoraireAlarme(client, j);
    }
    else{
      client.print(F("Alarme ")), client.print(j + 1), client.println(F(" desactivee"));
    }
    //Page alarme 2
    client.print(F("<form method=\"get\" action=\"pageReglageDeAlarme")), client.print(j + 1), client.print(F("\">"));         
    client.print(F("<input type=\"submit\" value=\"Regler Alarme ")), client.print(j + 1), client.println(F("\" />"));          
    client.println(F("</form>"));
  }

  if(Lampe.etat == 0){
    //Eclairer
    HTML_boutonFormulaire(client, "EclairerLampe1", "Eclairer");
  }

  if(Lampe.etat == 1){
    //Eteindre
    HTML_boutonFormulaire(client, "EteindreLampe1", "Eteindre");
  }

  //Actualiser
  HTML_boutonFormulaire(client, "Actualiser", "Actualiser");
}

void afficherHoraireAlarme(EthernetClient client, int numeroAlarme)
{
  client.print(hour(Alarm.read(Lampe.alarmes[numeroAlarme].idAlarme)));
  client.print(F(":"));
  client.print(minute(Alarm.read(Lampe.alarmes[numeroAlarme].idAlarme)));
  client.print(F(":"));
  client.println(second(Alarm.read(Lampe.alarmes[numeroAlarme].idAlarme)));
}

void afficherHeure(EthernetClient client){
  client.print(hour());
  client.print(":");
  client.print(minute());
  client.print(":");
  client.println(second());
}

void MorningAlarm(){
  digitalWrite(Lampe.pin, HIGH);  
  Lampe.etat = 1;
}

void EveningAlarm(){
  digitalWrite(Lampe.pin, LOW);  
  Lampe.etat = 0;
}


