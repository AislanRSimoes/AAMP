/* AAMP - validation/browser/tc059_normative_applicability.js
   TC-059 -- Coerencia entre aplicabilidade declarada e apresentacao normativa (P2-002 / F3).

   REGRESSAO BROWSER-NATIVE DE MAQUINA-ALVO. NAO faz parte da suite headless do CTest:
   nao existe runner JavaScript headless no projeto. Ver README.md nesta pasta.

   NAO reimplementa politica: renderiza os builders PRODUTIVOS de resources/aamp_laboratory.html
   -- buildIntegrityEnv e buildNoiseEnv -- num stage real do DOM e le o texto resultante.

   Antes do P2-002, a aba Integridade exibia os gauges e chips de SNR e Consistencia
   INCONDICIONALMENTE e, logo abaixo, declarava "Sinal externo -- metricas normativas
   (SNR, consistencia) nao se aplicam". A interface afirmava as duas coisas ao mesmo tempo.
   O instrumento Ruido reapresentava Piso/SNR/Faixa Dinamica como resultados normativos.

   O discriminador e o `hasAnalysis` que JA existia; nenhum limiar novo foi criado. */
(function(){
  'use strict';
  if(typeof buildIntegrityEnv!=='function'||typeof buildNoiseEnv!=='function')
    throw new Error('TC-059 FALHOU: builders nao carregados. Abra resources/aamp_laboratory.html antes.');

  var S=function(k,v,q,vp){return {key:k,value:v,unit:'dB',
                                   quality:(q||'Normal'),
                                   valuePresent:(vp===undefined?true:vp)};};
  function sess(hasA,sc){return {
    metrics:{byId:{'MS-04':{id:'MS-04',quality:'Normal',scalars:[],curves:[]}},
             scalars:sc,curves:{},lowConf:0,na:0},
    _integ:{q:{peakDb:-12,dc:0,clip:0,silPct:0,rmsDb:-20,crest:8},clipTot:0},
    _trust:'ok',captures:[],primary:0,hasAnalysis:hasA,provenance:{},acquisition:null};}
  function paint(b){var st=document.createElement('div');document.body.appendChild(st);
    var t;try{b(st);t=st.textContent||'';}catch(e){t='EXCECAO: '+e.message;}finally{st.remove();}
    return t.replace(/\s+/g,' ');}

  // Escalares NUMERICAMENTE VALIDOS plantados de proposito: o teste prova que valores validos
  // nao bastam para aparecer quando a sessao declara que a analise normativa nao se aplica.
  var VALIDOS={'SNR_dB':S('SNR_dB',62),
               'RepetitionConsistency_pct':S('RepetitionConsistency_pct',1.5),
               'NoiseFloor_dBFS':S('NoiseFloor_dBFS',-95),
               'DynamicRange_dB':S('DynamicRange_dB',86)};

  var pass=0,fail=0,lines=[];
  function check(n,cond){cond?pass++:fail++;lines.push((cond?'PASS':'FAIL')+' | '+n);}

  var Morig=M;
  try{
    // ---- A: sessao EXTERNA / nao normativa ----
    M=sess(false,VALIDOS);
    var aI=paint(buildIntegrityEnv), aN=paint(buildNoiseEnv);
    check('A1 · Integridade nao exibe SNR',                 aI.indexOf('62')<0);
    check('A2 · Integridade nao exibe Consistencia',        aI.indexOf('1.5')<0);
    check('A3 · Integridade mantem descritores BRUTOS',     /HEADROOM/.test(aI)&&/-12/.test(aI)&&/CRISTA/.test(aI)&&/CLIPPING/.test(aI));
    check('A4 · Integridade mantem aviso de nao aplicabilidade', /não se aplicam/.test(aI));
    check('A5 · Ruido nao exibe Piso/SNR',                  aN.indexOf('-95')<0&&aN.indexOf('62')<0);
    check('A6 · Ruido declara indisponivel',                /indispon/.test(aN));
    // NAVEGACAO deve concordar com o builder: se o instrumento mostra "sem dados",
    // o LED nao pode acender 'ok' so porque existe um escalar no contrato.
    check('A7 · hasData(noise) === false',                  hasData('noise')===false);
    check('A8 · domLed(noise) === idle',                    domLed('noise')==='idle');

    // ---- B: sessao NORMATIVA, MESMOS escalares ----
    M=sess(true,VALIDOS);
    var bI=paint(buildIntegrityEnv), bN=paint(buildNoiseEnv);
    check('B1 · Integridade exibe SNR',                     bI.indexOf('62')>=0);
    check('B2 · Integridade exibe Consistencia',            bI.indexOf('1.5')>=0);
    check('B3 · Ruido exibe Piso e SNR',                    bN.indexOf('-95')>=0&&bN.indexOf('62')>=0);
    check('B4 · Integridade sem aviso de nao aplicabilidade', !/não se aplicam/.test(bI));
    check('B5 · hasData(noise) === true',                   hasData('noise')===true);
    check('B6 · domLed(noise) === ok',                      domLed('noise')==='ok');

    // ---- C: P2-001 continua vigente dentro da sessao normativa ----
    M=sess(true,{'SNR_dB':S('SNR_dB',-300,'NotApplicable',false),
                 'NoiseFloor_dBFS':S('NoiseFloor_dBFS',-300,'NotApplicable',false)});
    var cI=paint(buildIntegrityEnv), cN=paint(buildNoiseEnv);
    check('C1 · NotApplicable segue oculto (P2-001)',       cI.indexOf('-300')<0&&cN.indexOf('-300')<0);
    check('C2 · sem NaN/Infinity',                          !/NaN|Infinity/.test(cI+cN));

    // ---- D: ausencia real de dados ----
    M=sess(false,{});
    var dI=paint(buildIntegrityEnv), dN=paint(buildNoiseEnv);
    check('D1 · sem dados nao lanca excecao',               dI.indexOf('EXCECAO')<0&&dN.indexOf('EXCECAO')<0);

    // ---- E: PROVA DE DENTES -- discriminante ----
    // A e B tem escalares IDENTICOS e diferem apenas em hasAnalysis. No comportamento anterior
    // os dois exibiriam os valores; hoje divergem. Se alguem remover o gate, E1 falha.
    check('E1 · hasAnalysis governa a RENDERIZACAO: A oculta, B exibe',
          (aI.indexOf('62')<0)&&(bI.indexOf('62')>=0)&&
          (aN.indexOf('-95')<0)&&(bN.indexOf('-95')>=0));
    // A divergencia tem de aparecer TAMBEM na navegacao, nao so no texto renderizado.
    M=sess(false,VALIDOS); var navA={h:hasData('noise'),l:domLed('noise')};
    M=sess(true,VALIDOS);  var navB={h:hasData('noise'),l:domLed('noise')};
    check('E2 · hasAnalysis governa a NAVEGACAO: A idle, B ok',
          navA.h===false&&navA.l==='idle'&&navB.h===true&&navB.l==='ok');
  } finally { M=Morig; }

  lines.push('');
  lines.push('==== '+pass+' passaram, '+fail+' falharam ====');
  var out=lines.join('\n');
  if(typeof console!=='undefined'&&console.log)console.log(out);
  if(fail>0)throw new Error('TC-059 FALHOU: '+fail+' caso(s) divergiram.\n'+out);
  return out;
})();
