# Alteracoes de Tempo Real no NKE

Este repositorio reune os codigos desenvolvidos na disciplina **Sistemas de Tempo Real**, do curso de **Engenharia de Computacao da UERGS**.

A proposta e usar o **NKE (Nano Kernel Educational)** como base e registrar, de forma incremental, as alteracoes feitas para aproximar esse nanokernel educacional de conceitos e tecnicas de sistemas de tempo real.

O objetivo deste README nao e reexplicar o NKE base. A arquitetura original, a motivacao educacional e os detalhes internos do NKE devem ser consultados nos artigos, videos e repositorios originais listados nas referencias. Aqui, o foco e documentar as adaptacoes feitas durante a disciplina.

## Implementacao

A implementacao esta sendo feita em um sketch Arduino, no arquivo [`nke-base.ino`](./nke-base.ino).

O codigo parte de uma versao do NKE adaptada para Arduino/AVR e deve evoluir ao longo da disciplina conforme novas tecnicas de tempo real forem estudadas e implementadas.

Cada parte da implementacao de tempo real sera desenvolvida em uma branch separada do projeto, facilitando a comparacao entre abordagens e mantendo o historico de cada tecnica isolado.

Nos testes, podem ser usados:

- **Arduino IDE**: https://www.arduino.cc/en/software
- **Wokwi**: https://wokwi.com/

O Wokwi permite simular projetos Arduino diretamente pelo navegador, sendo util para testar e observar o comportamento do kernel sem depender sempre da placa fisica.

## Referencias do NKE

- Costa, Celso Maciel da; Fragoso, Joao Leonardo; Matias Jr., Lucas Rivalino; Silva, Leonardo da Luz; Fracalossi, Aline; Brasil, Cassio; Debom, Guilherme. **NKE - Um Nanokernel Educacional para Microprocessadores ARM**. Disponivel em: https://www.academia.edu/13003555/NKE_Um_Nanokernel_Educacional_para_Microprocessadores_ARM
- Costa, Celso Maciel da; Fragoso, Joao Leonardo; Matias Jr., Lucas Rivalino; Silva, Leonardo da Luz; Fracalossi, Aline; Brasil, Cassio; Debom, Guilherme. **NKE - Um Nanokernel Educacional para Microprocessadores ARM**. Anais do SBESC 2014. Disponivel em: https://sbesc.lisha.ufsc.br/sbesc2014/dl225
- jeisonmp. **NKE0.8a - Nanokernel para ARM-LPC2378, versao 0.8a**. Disponivel em: https://github.com/jeisonmp/NKE0.8a
- Portal de Periodicos da Univali. **Referencia adicional relacionada aos materiais indicados sobre NKE**. Disponivel em: https://periodicos.univali.br/index.php/acotb/article/view/21088/12159
- **Video relacionado ao NKE**. Disponivel em: https://www.youtube.com/watch?v=pD51msNip78

## Referencias de apoio

- Oliveira, Romulo Silva de. **Sistemas de Tempo Real**.
- Liu, C. L.; Layland, J. W. **Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment**.

## Instituicao

**Universidade Estadual do Rio Grande do Sul (UERGS)**  
Curso: **Engenharia de Computacao**  
Disciplina: **Sistemas de Tempo Real**
