/* AAMP - validation/browser/tc064_semantic_honesty.js
   TC-064 -- Honestidade semantica: V3 (rotulo), V4 (placeholder nao e medicao) e
   asserçoes de fase. Ratificado em AAMP-HG-20260811-001 (alternativa B).

   REGRESSAO BROWSER-NATIVE DE MAQUINA-ALVO. NAO faz parte da suite headless do CTest:
   nao existe runner JavaScript headless no projeto. Ver README.md nesta pasta.

   NAO reimplementa politica. Usa as funcoes PRODUTIVAS de resources/aamp_laboratory.html:
   LabEngine.run (emissao real do contrato), buildIntegrityEnv (renderizacao real),
   scanAnomalies, correlate, recommend, kb e scalarForDisplay.

   O QUE ESTAVA ERRADO ANTES:
   - V3: RepetitionConsistency_pct e um COEFICIENTE DE VARIACAO (0% = perfeito), mas era
     exibido num gauge 0->100 rotulado "Consistencia", que le visualmente maior = melhor.
   - V4: Reproducibility_pct e um PLACEHOLDER constante, publicado com Quality::Normal +
     valuePresent=true + EstimateNature::Point -- indistinguivel de uma medicao pontual
     valida nos TRES eixos ratificados no P1-009 estagio 1.
   - Fase: cinco superficies afirmavam "coincide em fase" / "fase minima" / "corrigivel por
     EQ" a partir de uma simples variacao de fase medida por limiar NAO RATIFICADO.

   O QUE ESTE TESTE **NAO** COBRE, DELIBERADAMENTE:
   os limiares 20 graus / 25 graus, suas janelas e os demais thresholds do inventario
   permanecem NAO RATIFICADOS e INALTERADOS -- vao para o estudo normativo V5/DV-4. O teste
   prova justamente que eles NAO mudaram: so a afirmacao mudou. */
(function(){
  'use strict';
  var need=['LabEngine','buildIntegrityEnv','scanAnomalies','correlate','recommend','kb','scalarForDisplay','rangeStats'];
  for(var i=0;i<need.length;i++){
    if(eval('typeof '+need[i])==='undefined')
      throw new Error('TC-064 FALHOU: '+need[i]+' nao carregado. Abra resources/aamp_laboratory.html antes.');
  }

  var pass=0,fail=0,lines=[];
  function check(n,cond){cond?pass++:fail++;lines.push((cond?'PASS':'FAIL')+' | '+n);}
  // Precondicao: falha ALTO se a montagem do fixture nao produzir o que o caso pressupoe.
  // Sem isto um teste pode "passar" porque nada foi exercitado (licao dos passes vacuos).
  function must(n,cond){if(!cond)throw new Error('TC-064 ABORTADO: precondicao falsa -- '+n);}

  function S(k,v,q,vp,u){return {key:k,value:v,unit:u||'dB',
                                 quality:(q||'Normal'),
                                 valuePresent:(vp===undefined?true:vp)};}
  function sess(hasA,sc,curves){return {
    metrics:{byId:{'MS-04':{id:'MS-04',quality:'Normal',scalars:[],curves:[]}},
             scalars:sc,curves:(curves||{}),lowConf:0,na:0},
    _integ:{q:{peakDb:-12,dc:0,clip:0,silPct:0,rmsDb:-20,crest:8},clipTot:0},
    _trust:'ok',captures:[],primary:0,hasAnalysis:hasA,provenance:{},acquisition:null};}
  function paint(b){var st=document.createElement('div');document.body.appendChild(st);
    var t;try{b(st);t=st.textContent||'';}catch(e){t='EXCECAO: '+e.message;}finally{st.remove();}
    return t.replace(/\s+/g,' ');}

  var Morig=M, LANGorig=LANG, txt=[];   // txt acumula TODO texto produzido, para a varredura final

  try{
    /* ================= A -- V3: rotulo neutro em pt-BR ================= */
    LANG='pt-BR';
    var VAL={'SNR_dB':S('SNR_dB',62),
             'RepetitionConsistency_pct':S('RepetitionConsistency_pct',1.5,'Normal',true,'%')};
    M=sess(true,VAL);
    var aI=paint(buildIntegrityEnv); txt.push(aI);
    must('A -- Integridade renderizou algo', aI.length>20);
    check('A1 · chip/gauge usam VARIACAO',            /VARIAÇÃO/.test(aI)&&/Variação/.test(aI));
    check('A2 · nao resta CONSISTENCIA',              !/CONSISTÊNCIA/.test(aI)&&!/Consistência/.test(aI));
    check('A3 · valor do CV exibido SEM transformacao', aI.indexOf('1.5')>=0&&aI.indexOf('98.5')<0);

    /* ================= B -- V3: rotulo neutro em en ================= */
    LANG='en';
    M=sess(true,VAL);
    var bI=paint(buildIntegrityEnv); txt.push(bI);
    must('B -- Integridade renderizou algo em en', bI.length>20);
    check('B1 · traducao usa VARIATION',              /VARIATION/.test(bI)&&/Variation/.test(bI));
    check('B2 · nao resta CONSISTENCY',               !/CONSISTENCY/.test(bI)&&!/Consistency/.test(bI));
    check('B3 · mesmo valor do CV em en',             bI.indexOf('1.5')>=0);
    LANG='pt-BR';

    /* ================= C -- V4: contrato REAL emitido pelo LabEngine =================
       Nao e fixture: LabEngine.run e o emissor produtivo. Uma sequencia ST-15 sintetica
       basta para MS-15 sair de 'Error' e a familia ser montada. */
    var fs=48000, n=fs*13, seq=new Float32Array(n);
    for(var s=0;s<n;s++) seq[s]=0.2*Math.sin(2*Math.PI*440*s/fs);
    var bundle=LabEngine.run({captures:[{stimulus:'ST-15',wav:{samples:seq,sampleRate:fs}}],stimuli:[]});
    var m15=bundle.byId&&bundle.byId['MS-15'];
    must('C -- MS-15 foi emitida pelo LabEngine', !!m15);
    var rep=null,lin=null;
    for(var j=0;j<m15.scalars.length;j++){
      if(m15.scalars[j].key==='Reproducibility_pct')rep=m15.scalars[j];
      if(m15.scalars[j].key==='AggregateLinearity_dB')lin=m15.scalars[j];
    }
    must('C -- escalar Reproducibility_pct presente', !!rep);
    check('C1 · chave preservada',                   rep.key==='Reproducibility_pct');
    check('C2 · valor bruto 100 preservado',         rep.value===100.0);
    check('C3 · quality === NotApplicable',          rep.quality==='NotApplicable');
    check('C4 · valuePresent === false',             rep.valuePresent===false);
    check('C5 · nenhum EstimateNature novo criado',  rep.estimate===undefined||rep.estimate==='Point');
    check('C6 · familia MS-15 continua Normal',      m15.quality==='Normal');
    check('C7 · AggregateLinearity segue medicao valida',
          !!lin&&lin.valuePresent===true&&lin.quality==='Normal'&&isFinite(lin.value));

    /* ================= D -- V4 nao chega a apresentacao ================= */
    check('D1 · scalarForDisplay rejeita o placeholder', scalarForDisplay(rep)===null);
    // discriminante: o MESMO 100.0 com contrato valido SERIA exibido -- prova que quem
    // governa e quality/valuePresent, nao o numero.
    check('D2 · discriminante: 100 com contrato valido seria exibido',
          scalarForDisplay(S('Reproducibility_pct',100.0,'Normal',true,'%'))===100.0);

    /* ================= E -- V3: chave e valor do contrato intactos ================= */
    // MS-13 e familia COMPOSTA: ST-07 (burst) e obrigatorio; ST-12 (trem de pulsos)
    // e o que alimenta RepetitionConsistency_pct. Os dois precisam existir.
    var burst=new Float32Array(Math.round(0.2*fs));
    for(var s7=0;s7<Math.round(0.1*fs);s7++)burst[s7]=0.5*Math.sin(2*Math.PI*1000*s7/fs);
    var pulses=new Float32Array(fs*2);
    for(var p=0;p<10;p++){var b0=Math.round(p*0.1*fs);
      for(var q=0;q<Math.round(0.02*fs);q++)pulses[b0+q]=0.5*Math.sin(2*Math.PI*1000*q/fs);}
    var b12=LabEngine.run({captures:[{stimulus:'ST-07',wav:{samples:burst,sampleRate:fs}},
                                     {stimulus:'ST-12',wav:{samples:pulses,sampleRate:fs}}],stimuli:[]});
    var cv=b12.scalars&&b12.scalars['RepetitionConsistency_pct'];
    must('E -- MS-13 emitiu RepetitionConsistency_pct', !!cv);
    check('E1 · chave de contrato inalterada',       cv.key==='RepetitionConsistency_pct');
    check('E2 · valor NAO invertido (nao e 100-CV)', typeof cv.value==='number'&&isFinite(cv.value)&&cv.value>=0);

    /* ================= F/G -- fase: FRONTEIRAS EXATAS =================
       Os limiares 25 (anomalia, janela 0,7..1,4x) e 20 (correlato, janela 0,8..1,25x)
       continuam NAO RATIFICADOS. Este bloco NAO os ratifica: fixa que eles nao MUDARAM.

       Construcao: a fase e um DEGRAU de 0 para V exatamente em 1 kHz -- o centro do pico.
       Qualquer janela que contenha 1 kHz e pontos dos dois lados tem ripple = V EXATAMENTE,
       independente de qual janela o codigo produtivo escolha (pk.fL..pk.fR ou 0,7..1,4x).
       Isso permite testar a fronteira sem depender da largura interna do detector.
       As comparacoes produtivas sao ESTRITAS (ripple>25, ripple>20): em V == limiar,
       nada deve aparecer; um passo acima, deve aparecer. */
    var fx=[],fy=[];
    for(var k=0;k<400;k++){
      var f=20*Math.pow(1000,k/399);                       // 20 Hz .. 20 kHz log
      fx.push(f);
      var d=Math.log2(f/1000);
      fy.push(14*Math.exp(-(d*d)/(2*0.18*0.18)));          // bump +14 dB, sigma 0.18 oct
    }
    // degrau exato em 1 kHz: ripple = V em qualquer janela que atravesse o centro
    function phaseStep(V){return {x:fx,y:fx.map(function(f){return f<1000?0:V;})};}
    function curvesFor(V){return {'MS-01':{x:fx,y:fy},'MS-02':phaseStep(V)};}
    // Sufixo da anomalia de Ressonancia, para uma dada amplitude de degrau de fase.
    function sufixoAnomalia(V){
      M=sess(true,{},curvesFor(V));
      var a=scanAnomalies(M).filter(function(x){return /Ressonância/.test(x.label);})[0];
      must('F -- anomalia de Ressonancia detectada em V='+V, !!a);
      txt.push(a.label+' '+a.detail);
      return a.detail;
    }
    // Correlato de fase, para uma dada amplitude de degrau de fase.
    function correlatoFase(V){
      M=sess(true,{},curvesFor(V));
      var c=correlate({dom:'frequency',f:1000,type:'peak'}).filter(function(x){return x.dom==='phase';})[0];
      if(c)txt.push(c.label+' '+c.detail);
      return c||null;
    }
    /* nextUp(x): menor double ESTRITAMENTE maior que x (x > 0). Incrementa em 1 ULP a
       representacao inteira do bit pattern IEEE-754. E o que permite provar valor EXATO,
       e nao intervalo: entre x e nextUp(x) NAO EXISTE outro double.
         V = L         nao dispara  => L >= V        (a comparacao produtiva e `ripple > L`)
         V = nextUp(L) dispara      => L <  nextUp(L)
       Como nao ha double entre os dois, as duas condicoes juntas forcam L === V. */
    function nextUp(x){
      var f=new Float64Array(1), u=new BigUint64Array(f.buffer);
      f[0]=x; u[0]+=1n; return f[0];
    }
    var UP25=nextUp(25), UP20=nextUp(20);
    // Adjacencia verificavel: o ponto medio entre dois doubles ADJACENTES nao e representavel,
    // logo arredonda para um dos dois extremos. Se houvesse double entre eles, o ponto medio
    // seria distinto de ambos e esta precondicao falharia.
    function adjacente(a,b){var m=(a+b)/2; return b>a && (m===a||m===b);}
    must('F -- nextUp(25) e o proximo double representavel', adjacente(25,UP25));
    must('F -- nextUp(20) e o proximo double representavel', adjacente(20,UP20));

    // Precondicao: o ripple medido nas DUAS janelas tem de ser EXATAMENTE V.
    // Se esta igualdade nao valer, o teste de fronteira nao teria significado.
    [20,UP20,20.5,25,UP25,25.5,40].forEach(function(V){
      must('F -- ripple exato na janela da anomalia (V='+V+')',
           rangeStats(phaseStep(V),700,1400).ripple===V);
      must('F -- ripple exato na janela do correlato (V='+V+')',
           rangeStats(phaseStep(V),800,1250).ripple===V);
    });

    /* ---- F: fronteira do limiar de 25 (anomalia) ---- */
    var dEq25=sufixoAnomalia(25), dUp25=sufixoAnomalia(UP25),
        dGt25=sufixoAnomalia(25.5), dZero=sufixoAnomalia(0);
    check('F1 · acima de 25 gera sufixo neutro',      /variação de fase/.test(dGt25));
    check('F2 · sufixo traz o ripple observado',      /variação de fase de 26°/.test(dGt25));
    check('F3 · nao afirma coincidencia',             !/coincide/i.test(dGt25));
    check('F4 · fase nula nao gera sufixo',           !/variação de fase/.test(dZero));
    // A comparacao produtiva e ESTRITA: em exatamente 25 nada pode aparecer => limiar >= 25.
    check('F5 · em 25 nao ha sufixo (limiar >= 25)',  !/variação de fase/.test(dEq25));
    // No PROXIMO double, ja aparece => limiar < nextUp(25).
    check('F6 · em nextUp(25) ha sufixo (limiar < nextUp(25))', /variação de fase/.test(dUp25));
    // Como nao existe double entre 25 e nextUp(25), F5 e F6 juntos forcam limiar === 25.
    // Qualquer alteracao do literal produtivo -- inclusive 25.25 -- derruba um dos dois.
    check('F7 · limiar e EXATAMENTE 25 (F5 e F6 juntos)',
          !/variação de fase/.test(dEq25) && /variação de fase/.test(dUp25));

    /* ---- G: fronteira do limiar de 20 (correlato) ---- */
    var cEq20=correlatoFase(20), cUp20=correlatoFase(UP20), cGt20=correlatoFase(20.5);
    must('G -- correlato produzido acima de 20', !!cGt20);
    check('G1 · rotulo neutro',                       cGt20.label==='Variação de fase');
    check('G2 · nao afirma coincidencia',             !/[Cc]oincide/.test(cGt20.label+cGt20.detail));
    check('G3 · nao conclui fase minima',             !/fase mínima/.test(cGt20.detail));
    // o numero exibido e o ripple REAL da janela 0,8..1,25x, nao um rotulo fixo
    check('G4 · mantem o valor observado da janela',  cGt20.detail.indexOf('21°')===0);
    check('G5 · em 20 nao ha correlato (limiar >= 20)',            cEq20===null);
    check('G6 · em nextUp(20) ha correlato (limiar < nextUp(20))', cUp20!==null);
    check('G7 · limiar e EXATAMENTE 20 (G5 e G6 juntos)',          cEq20===null&&cUp20!==null);

    /* ================= H -- recomendacao e KB ================= */
    var rec=recommend({dom:'frequency',label:'Ressonância em 1.00 kHz',focus:{f:1000}});
    must('H -- recommend produziu texto', !!rec&&!!rec.text);
    txt.push(rec.opt+' '+rec.text);
    check('H1 · nao afirma fase minima como conclusao', !/^(?!.*só um sistema).*é de fase mínima/.test(rec.text));
    check('H2 · nao prescreve EQ como conclusao demonstrada', !/Coincidindo em fase/.test(rec.text));
    check('H3 · recomenda verificacao',              /confronte|verificadamente/.test(rec.text));
    var kbr=kb('resonance');
    must('H -- KB resonance existe', !!kbr);
    txt.push(kbr.t+' '+kbr.d);
    check('H4 · KB nao infere fase minima',          /não demonstra/.test(kbr.d));
    check('H5 · KB nao promete correcao por EQ inverso', !/e corrigível por EQ inverso/.test(kbr.d));

    /* ================= I -- varredura negativa sobre TODO texto produzido ================= */
    var all=txt.join(' || ');
    var proibidas=['coincide em fase','Coincide em fase','coincides in phase','Coincides in phase',
                   'Coincidindo em fase','Coinciding in phase'];
    var achadas=proibidas.filter(function(s){return all.indexOf(s)>=0;});
    check('I1 · nenhuma asserçao de coincidencia em fase', achadas.length===0);
    // "fase minima" so pode aparecer em formulacao CONDICIONAL/NEGATIVA, nunca afirmativa.
    var afirmativa=/(é de fase mínima|it is minimum-phase)/.test(all);
    check('I2 · nenhuma afirmacao de fase minima',   afirmativa===false);
    if(achadas.length)lines.push('     achadas: '+achadas.join(' ; '));

  } finally { M=Morig; LANG=LANGorig; }

  console.log(lines.join('\n'));
  console.log('TC-064 -- '+pass+'/'+(pass+fail)+' checks OK');
  if(fail)throw new Error('TC-064 FALHOU: '+fail+' de '+(pass+fail)+' checks');
  return {pass:pass,fail:fail,total:pass+fail};
})();
