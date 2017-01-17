/*
Projekt: Cementarna
Autoři: Petr Mather (xmathe00), Petra Heczkova (xheczk04)

Projekt simuluje těžbu vápence a následnou pridruženou výrobu portlandského cementu.
Další informace a popis viz dokumentace.

jednotky času jsou minuty, základní jednotka je tuna materiálu/výrobku.
*/
#include "simlib.h"
#include "math.h"
#include "stdio.h"

//některé větší jednotky
#define HODINA 60
#define DEN 1440

//stroje v lomu
Facility rypadlo("Rypadlo");//rypadlo vyvážející kusy z lomu na drcení
Facility drtic_cel("Čelisťový drtič");//čelisťový drtič pro drcení velkých kusu
Facility drtic_kuz("Kuželový drtič");//kuželový drtič pro jemnější drcení
Facility drtic_kuz2("Druhý kuželový drtič");//druhý, přidaný v průběhu optimalizace

//nakladani
Facility dulni_nakladac("Důlní nakladač");//nakladač materiálu na auta
Facility nakladaci_misto("Nakládací místo u dolu");//místo pro auto pod nakladačem


//stroje v tovarne
Store predehrivac("Předehřívání", 5);
Facility pec[15];//samotná pec, rozdělená na "pole pecí" pro simulování spojitého zpracování
Facility mlyn;//mlýn za pecí na mletí slinku na cement
Facility balicka;//pytlování výrobku

int misto_v_CATu;//počítadlo volné nosnosti nakládaného auta
int cislo_pokusu;

/*
***třída popisující chování tuny materiálu v lomu
*/
class Ton: public Process{
  void Behavior(){

    //nabírání materiálu do třídícího síta
    Seize(rypadlo);
    Wait(1);
    Release(rypadlo);

    double hrubost = Random();//síto, rozdělení podle potřeby drcení

    //síto, třídí podle průměru částic
    if(hrubost >= 0.4)
    {//je potřeba rozdrtit z velkých kusů
      Seize(drtic_cel);
      Wait(Uniform(0.2,0.5));
      Release(drtic_cel);
    }
    if(hrubost >= 0.1)
    {//je potřeba rozdrtit jemněji
      if(cislo_pokusu <= 4)
      {//v případě pozdějších testů, kdy máme dva
	Seize(drtic_kuz);
	Wait(Uniform(1.6,2.5));
	Release(drtic_kuz);
      }
      else
      {
	if(drtic_kuz.QueueLen() > drtic_kuz2.QueueLen())
	{
	  Seize(drtic_kuz2);
	  Wait(Uniform(1.6,2.5));
	  Release(drtic_kuz2);
	}
	else
	{
	  Seize(drtic_kuz);
	  Wait(Uniform(1.6,2.5));
	  Release(drtic_kuz);
	}

      }
    }
    //rozdrcené pro přepravu

    Seize(dulni_nakladac);
    if(misto_v_CATu >= 0)//pokud je místo, nakládáme
    {
      if(misto_v_CATu == 0)//pokud je auto plné
      {
	Passivate();//čekáme dokud "nevzbudí" auto které zaujme nakládací pozici
      }
      Wait(1);
      misto_v_CATu = misto_v_CATu - 1;
      if(misto_v_CATu == 0)//
      {
	(nakladaci_misto.in)->Activate();
      }
      Release(dulni_nakladac);
      Terminate();
    }
  }
};


/*
***třída popisující chování tuny materiálu v továrně
*/
class Ton_in: public Process{
  void Behavior(){
//začínáme ve vstupním silu továrny

//predehříváni materiálu do pece
    Enter(predehrivac, 1);
    Wait(5);
    Leave(predehrivac, 1);


//pec je rozdělena na pole míst, simuluje se tím tunelová spojitost
    for(int i = 0; i <= 14; i++)
    {
      Seize(pec[i]);
      Wait(0.95);
      Release(pec[i]);
    }

//chlazení
    Wait(Exponential(20));

//mletí slinku na cement
    Seize(mlyn);
    Wait(1);
    Release(mlyn);

//mensi cast se na prodej balí do pytlů
    if(Random()<0.3){
      Seize(balicka);
      Wait(5);
    }
//a větší část na váhu
    Terminate();
  }
};

//třída popisuje noc a víkend, v kontextu přerušování práce v lomu
class Noc: public Process{
  int den;
  void Behavior(){
  Priority= 1;
  Wait(8*HODINA);
  den = 1;
  while(1)
    {
//začala noc a "zabere" stroje, nechává dojít aktuální úlohy
      Seize(rypadlo);
      Seize(dulni_nakladac);
      Seize(drtic_cel);
      Seize(drtic_kuz);
      if(den == 5)//je víkend, trvá až do pondělního rána
      {
        Wait((24+24+16)*HODINA);
        den = 1;
      }
      else
      {
      Wait(16*HODINA);
      den++;
      }
//začal pracovní den a všechny stroje jsou uvolněny pro práci
      Release(rypadlo);
      Release(dulni_nakladac);
      Release(drtic_cel);
      Release(drtic_kuz);

      Wait(8*HODINA);
    }
  }

};

//nákladní auto převážející materiál z lomu do továrny
class  CAT: public Process{
  int i;
  double nakladame;
  void Behavior()
  {
    while(1)
    {
      Seize(nakladaci_misto);//zaparkuje k nakladači
      misto_v_CATu = 70;//má 70 míst (nostnost 70 tun)
      if(dulni_nakladac.in != NULL)
      {
	(dulni_nakladac.in)->Activate();
      }
      Passivate();//a čeká na naložení
      Release(nakladaci_misto);//odjede od nakladače
      Wait(Uniform(8,10));//jede do továrny

      for(i = 0; i < 70; i++)
      {//vysypává vápenec do vstupního sila továrny
        (new Ton_in)->Activate();
      } 
      Wait(Uniform(5,7));//a jede zpátky do lomu
    }
  }
};


//generátor materiálu, simulace odstřelů
class Odstrel: public Event{
  int i;
  int odstrelene_mnozstvi;
  void Behavior(){
	// kolik jsme "odloupli" ze skalní stěny
    odstrelene_mnozstvi = (int)round(Uniform(3000, 5000));
    for(i = 0; i < odstrelene_mnozstvi; i++)
    {
     (new Ton)->Activate();
    }

    Activate(Time + (7 * DEN));//další odstřel za týden
  }
};



int main(int argc, char *argv[]){
  if (argc == 2)
  {//číslo pokusu
    switch(atoi(argv[1]))
    {//jednotlivé pokusy, 'make run' pro spuštění po řadě
      case 1:
	cislo_pokusu = 1;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	pec[0].Output();
	printf("Jak vidíme z této tabulky, pec je vytížena pouze z 13 procent.\n");
	printf("Našim cílem je zvýšit její vytížení.\n");
	printf("Zkusíme najít místo v systému, kde lze zefektivnit přísun materiálu:\n");
	printf("\n\n\n\n\n");
	break;
      case 2:
	cislo_pokusu = 2;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	predehrivac.Output();
	printf("Toto je tabulka využití predehřívací pece.\n");
	printf("Jak vidíme, její průměrné využití je zhruba 0.7 tun materiálu,\n");
	printf("zatímco může zpracovávat až 5 tun. Tedy je málo zatížena\n");
	printf("a problém se bude nacházet ještě před ní ve výrobním řetězci.\n");
	printf("\n\n\n\n\n");
	break;
      case 3:
	cislo_pokusu = 3;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	dulni_nakladac.Output();
	printf("Tady pozorujeme tabulku využití nakladače.\n");
	printf("Hodnoty jsou zkresleny tím, že odstávka po směně nebo\n");
	printf("přes víkend je modelována jako využití. Nicméně je vidět,\n");
	printf("že i tu je průměrná řada v řádu jednotek tun.\n");
	printf("Podíváme se dál.\n");
	printf("\n\n\n\n\n");
	break;
      case 4:
	cislo_pokusu = 4;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	drtic_cel.Output();
	drtic_kuz.Output();
	printf("Podle tabulek čelisťový drtič stíhá,\n");
	printf("zatímco kuželový nabírá zpoždění a pozdržel\n");
	printf("skoro polovinu materiálu.\n");
	printf("Zkusíme přidat druhý.\n");
	printf("\n\n\n\n\n");
	break;
      case 5:
	cislo_pokusu = 5;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	drtic_kuz.Output();
	drtic_kuz2.Output();
	printf("Druhý kuželový drtič je využit pouze z 20 procent.\n");
	printf("Dal by se tedy nahradit méně výkonným, to nyní ponechme stranou.\n");
	printf("Zjistíme zvýšení efektivity pece.\n");
	printf("\n\n\n\n\n");
	break;
      case 6:
	cislo_pokusu = 6;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	pec[0].Output();
	printf("Pec zvýšila účinnost o 5 procent.\n");
	printf("Vzhledem k tomu, že kuželový drtič zadržoval polovinu materiálu,\n");
	printf("musíme zjistit kde je materiál zdržen tentokrát.\n");
	printf("\n\n\n\n\n");
	break;
      case 7:
	cislo_pokusu = 7;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	dulni_nakladac.Output();
	printf("Nyní je materiál zdržen zde.\n");
	printf("Zjistíme, jak pomůže systému auto na dopravu materiálu navíc.\n");
	printf("\n\n\n\n\n");
	break;
      case 8:
	cislo_pokusu = 8;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	dulni_nakladac.Output();
	printf("Druhé auto na dopravu materiálu vyřešilo problém.\n");
	printf("Opět zkontrolujme vytížení pece.\n");
	printf("\n\n\n\n\n");
	break;
      case 9:
	cislo_pokusu = 9;
	Init(0, (1000*DEN));
	(new Odstrel)->Activate();
	(new CAT)->Activate();
	(new CAT)->Activate();
	(new Noc)->Activate();
	Run();
	pec[0].Output();
	printf("Pec nyní zvedla výkonnost na 22,6 procenta.\n");
	printf("Nikde v systému se v tomto nastavení netvoří zbytečné fronty.\n");
	printf("V tomto okamžiku je výroba limitována převážně možnostmi dolu.\n\n");;
	printf("---KONEC---\n\n");
	break;
    }

  }
  return (0);
}
