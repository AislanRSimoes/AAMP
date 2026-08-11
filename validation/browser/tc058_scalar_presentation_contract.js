/* AAMP - validation/browser/tc058_scalar_presentation_contract.js
   TC-058 -- Apresentacao honra o contrato do escalar (P2-001 / DV-8).

   REGRESSAO BROWSER-NATIVE DE MAQUINA-ALVO. NAO faz parte da suite headless do CTest:
   nao existe runner JavaScript headless no projeto. Ver README.md nesta pasta.

   NAO reimplementa a politica: invoca as funcoes PRODUTIVAS carregadas de
   resources/aamp_laboratory.html -- scalarForDisplay, val e LAB.getMetric.

   Antes do P2-001, as tres rotas filtravam APENAS finitude: um escalar em
   NotApplicable/Error ou com valuePresent=false tinha seu numero exibido como valor
   cientifico -- a sentinela -300 dBFS aparecia em instrumento, gauge, chip, anomalia e
   frase interpretativa. Mesma classe de DV-5/DV-10 na camada de apresentacao. */
(function(){
  'use strict';
  if(typeof scalarForDisplay!=='function')
    throw new Error('TC-058 FALHOU: scalarForDisplay nao esta carregada. Abra resources/aamp_laboratory.html antes.');
  if(typeof val!=='function')
    throw new Error('TC-058 FALHOU: val global nao esta carregada.');
  if(typeof LAB!=='object'||typeof LAB.getMetric!=='function')
    throw new Error('TC-058 FALHOU: LAB.getMetric nao esta carregada.');

  var S=function(v,q,vp){return {key:'PROBE',value:v,unit:'dB',
                                 quality:(q===undefined?'Normal':q),
                                 valuePresent:(vp===undefined?true:vp)};};

  // nome                              escalar                         esperado
  var CASES=[
    ['escalar ausente',                null,                           null],
    ['Normal / true / 42',             S(42),                          42],
    ['LowConfidence / true / 42',      S(42,'LowConfidence'),          42],
    ['NotApplicable / false / -300',   S(-300,'NotApplicable',false),  null],
    ['NotApplicable / true / -300',    S(-300,'NotApplicable',true),   null],
    ['Error / false / 0',              S(0,'Error',false),             null],
    ['Error / true / 42',              S(42,'Error',true),             null],
    ['Normal / false / 42',            S(42,'Normal',false),           null],
    ['Normal / true / null',           S(null),                        null],
    ['Normal / true / NaN',            S(NaN),                         null],
    ['Normal / true / +Infinity',      S(Infinity),                    null],
    ['Normal / true / -Infinity',      S(-Infinity),                   null],
    // A sentinela NUNCA e reconhecida pelo NUMERO: -300 legitimamente Normal/presente
    // continua visivel, porque nesse caso e medicao. O contrato governa, nao o valor.
    ['Normal / true / -300 (legitimo)',S(-300),                        -300]
  ];

  var pass=0,fail=0,lines=[];
  function check(nome,got,exp){
    var ok=(got===exp)||(typeof got==='number'&&typeof exp==='number'&&got===exp);
    ok?pass++:fail++;
    lines.push((ok?'PASS':'FAIL')+' | '+nome+' | esperado='+exp+' obtido='+got);}

  // ---- (a) funcao pura ----
  for(var i=0;i<CASES.length;i++){
    var got;
    try{got=scalarForDisplay(CASES[i][1]);}catch(e){got='EXCECAO: '+e.message;}
    check('scalarForDisplay · '+CASES[i][0],got,CASES[i][2]);}

  // ---- (b) equivalencia das TRES rotas ----
  // val() global delega. LAB.getMetric le por chave do contrato da sessao: numa pagina
  // recem-carregada M e null, entao a sessao e montada e restaurada ao final.
  var Morig=M;
  try{
    for(var j=0;j<CASES.length;j++){
      var e=CASES[j][1],exp=CASES[j][2],gv;
      try{gv=val(e);}catch(ex){gv='EXCECAO: '+ex.message;}
      check('val() · '+CASES[j][0],gv,exp);
      M={metrics:{byId:{},scalars:(e?{'PROBE':e}:{}),curves:{},lowConf:0,na:0},captures:[],primary:0};
      var gm;try{gm=LAB.getMetric('PROBE');}catch(ex2){gm='EXCECAO: '+ex2.message;}
      check('LAB.getMetric() · '+CASES[j][0],gm,exp);
    }

    // ---- (c) scanAnomalies: DISCRIMINANTE valor x contrato ----
    // Nao e possivel mutar scalarForDisplay (e `const`). A prova de dentes vem de comparar
    // o MESMO valor -300 sob contratos diferentes: se o numero governasse, os tres casos
    // abaixo se comportariam igual.
    var mkS=function(v,q,vp){return {key:'x',value:v,unit:'dBFS',quality:q,valuePresent:vp};};
    var mkM=function(sc){return {metrics:{byId:{'MS-04':{id:'MS-04',quality:'Normal',scalars:[],curves:[]}},
        scalars:sc,curves:{},lowConf:0,na:0},
        _integ:{q:{peakDb:-12,dc:0,clip:0,silPct:0,rmsDb:-20,crest:8},clipTot:0},
        captures:[],primary:0,hasAnalysis:true};};
    var run=function(sc){M=mkM(sc);return JSON.stringify(scanAnomalies(M)||[]);};

    var pos=run({'NoiseFloor_dBFS':mkS(-42,'Normal',true),'SNR_dB':mkS(3,'Normal',true),'DynamicRange_dB':mkS(5,'Normal',true)});
    check('scanAnomalies · controle POSITIVO gera achado',(pos!=='[]'),true);

    var legit=run({'NoiseFloor_dBFS':mkS(-300,'Normal',true),'SNR_dB':mkS(-300,'Normal',true),'DynamicRange_dB':mkS(-300,'Normal',true)});
    check('scanAnomalies · -300 com contrato VALIDO e medicao',(legit!=='[]'),true);

    var na=run({'NoiseFloor_dBFS':mkS(-300,'NotApplicable',false),'SNR_dB':mkS(-300,'NotApplicable',false),'DynamicRange_dB':mkS(-300,'NotApplicable',false)});
    check('scanAnomalies · MESMO -300 em NotApplicable e ignorado',(na==='[]'),true);
  } finally { M=Morig; }

  lines.push('');
  lines.push('==== '+pass+' passaram, '+fail+' falharam ====');
  var out=lines.join('\n');
  if(typeof console!=='undefined'&&console.log)console.log(out);
  if(fail>0)throw new Error('TC-058 FALHOU: '+fail+' caso(s) divergiram.\n'+out);
  return out;
})();
