/* AAMP - validation/browser/tc057_trust_ladder.js
   TC-057 -- Trust Ladder: porta inferior do MS-10 e precedencia de integridade (P1-008 / V1).

   REGRESSAO BROWSER-NATIVE DE MAQUINA-ALVO. NAO faz parte da suite headless do CTest: nao existe
   runner JavaScript headless no projeto (sem Node, sem engine vendorizado), e adicionar um seria
   dependencia nova -- fora do escopo do P1-008. A limitacao e conhecida e esta registrada.

   Este arquivo NAO reimplementa a politica: ele invoca a `classifyTrust` PRODUTIVA carregada de
   resources/aamp_laboratory.html. Se a funcao for perturbada, os casos divergem e o teste falha.

   Execucao: ver README.md nesta pasta. */
(function(){
  'use strict';
  if(typeof classifyTrust!=='function')
    throw new Error('TC-057 FALHOU: classifyTrust nao esta carregada. Abra resources/aamp_laboratory.html antes.');

  // ---- construtores de fixture (integridade da captura / contrato de metricas) ----
  // peakDb === null modela ausencia de integridade observavel (q ausente).
  var I=function(peakDb,clip){return {q:(peakDb===null?null:{peakDb:peakDb}),clipTot:clip||0};};
  var S=function(v,q,vp){return {key:'SNR_dB',value:v,unit:'dB',
                                 quality:(q===undefined?'Normal':q),
                                 valuePresent:(vp===undefined?true:vp)};};
  // fam: {fq:<quality da familia>, s:<escalar SNR_dB dentro da familia>} ou null (familia ausente).
  // gs: escalar 'SNR_dB' plantado no indice GLOBAL, para provar consumo exclusivo da familia.
  var M=function(fam,lc,gs){
    var byId={},scalars={};
    if(fam){byId['MS-10']={id:'MS-10',name:'SNR',quality:(fam.fq||'Normal'),
                           scalars:(fam.s?[fam.s]:[]),curves:[]};
            if(fam.s)scalars['SNR_dB']=fam.s;}
    if(gs)scalars['SNR_dB']=gs;
    return {byId:byId,scalars:scalars,curves:{},lowConf:lc||0,na:0};};

  // ---- matriz EC-01 ----
  var CASES=[
    // nome                                   integridade      contrato                                              esperado
    ['nominal',                               I(-12),   M({s:S(60)},0),                                              'ok'],
    ['periferico LowConfidence',              I(-12),   M({s:S(60)},1),                                              'warn'],
    ['MS-10 LowConfidence, SNR>0',            I(-12),   M({fq:'LowConfidence',s:S(60,'LowConfidence')},1),           'warn'],
    ['limite SNR=0',                          I(-12),   M({s:S(0)},0),                                               'bad'],
    ['abaixo do piso SNR<0',                  I(-12),   M({s:S(-5)},0),                                              'bad'],
    ['abaixo do piso + LowConfidence',        I(-12),   M({fq:'LowConfidence',s:S(-5,'LowConfidence')},1),           'bad'],
    ['escalar NotApplicable (sentinela -300)',I(-12),   M({s:S(-300,'NotApplicable',false)},0),                      'bad'],
    ['escalar Error',                         I(-12),   M({s:S(0,'Error',false)},0),                                 'bad'],
    ['escalar valuePresent=false',            I(-12),   M({s:S(50,'Normal',false)},0),                               'bad'],
    ['valor -Infinity',                       I(-12),   M({s:S(-Infinity)},0),                                       'bad'],
    ['valor NaN',                             I(-12),   M({s:S(NaN)},0),                                             'bad'],
    ['valor +Infinity',                       I(-12),   M({s:S(Infinity)},0),                                        'bad'],
    ['valor null',                            I(-12),   M({s:S(null)},0),                                            'bad'],
    // --- estado da FAMILIA governa antes do escalar ---
    ['familia NotApplicable, escalar "valido"',I(-12),  M({fq:'NotApplicable',s:S(60)},0),                           'bad'],
    ['familia Error, escalar "valido"',       I(-12),   M({fq:'Error',s:S(60)},0),                                   'bad'],
    // familia LowConfidence com escalar Normal e agregado ZERO: ressalva explicita, nao pode ser 'ok'
    ['familia LowConf, escalar Normal, lc=0', I(-12),   M({fq:'LowConfidence',s:S(60)},0),                           'warn'],
    // --- consumo EXCLUSIVO da familia: escalar global nao alimenta a porta ---
    ['familia sem SNR_dB + global presente',  I(-12),   M({s:null},0,S(60)),                                         'bad'],
    // --- MS-10 ausente preserva o comportamento anterior (EC-01 12D) ---
    ['MS-10 ausente',                         I(-12),   M(null,0),                                                   'ok'],
    ['MS-10 ausente + LowConfidence',         I(-12),   M(null,1),                                                   'warn'],
    ['MS-10 ausente, sem integridade',        I(null),  M(null,0),                                                   'idle'],
    // --- porta superior preservada e dominante ---
    ['clipping',                              I(-12,3), M({s:S(60)},0),                                              'bad'],
    ['clipping domina SNR valido',            I(-12,1), M({s:S(60)},5),                                              'bad'],
    ['pico critico > -1 dBFS',                I(-0.5),  M({s:S(60)},0),                                              'bad'],
    ['pico baixo entre -6 e -1',              I(-3),    M({s:S(60)},0),                                              'warn'],
    // --- severidade: bad domina warn (EC-01 12C) ---
    ['bad da porta domina lowConf agregado',  I(-12),   M({s:S(-5)},9),                                              'bad'],
    // --- lowConf==0 nao promove a ok quando MS-10 e inutilizavel (EC-01 12B) ---
    ['lowConf=0 nao salva NotApplicable',     I(-12),   M({s:S(-300,'NotApplicable',false)},0),                      'bad']
  ];

  var pass=0,fail=0,lines=[];
  for(var i=0;i<CASES.length;i++){
    var name=CASES[i][0],got;
    try{got=classifyTrust(CASES[i][1],CASES[i][2]);}
    catch(e){got='EXCECAO: '+e.message;}
    var exp=CASES[i][3],ok=(got===exp);
    ok?pass++:fail++;
    lines.push((ok?'PASS':'FAIL')+' | '+name+' | esperado='+exp+' obtido='+got);
  }
  lines.push('');
  lines.push('==== '+pass+' passaram, '+fail+' falharam ====');
  var out=lines.join('\n');
  if(typeof console!=='undefined'&&console.log)console.log(out);
  if(fail>0)throw new Error('TC-057 FALHOU: '+fail+' caso(s) divergiram.\n'+out);
  return out;
})();
