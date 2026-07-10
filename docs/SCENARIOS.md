# Scenario Guide

Los escenarios integrados son fixtures deterministas. Sirven para probar el
contrato del CLI y para comparar snapshots al escribir una mitigacion.

## `snapshot`

Registra dos accounts y dos vaults sin actividad economica. Es util para
comprobar la forma del JSON y el digest de estado minimo.

Propiedades esperadas:

- cero redenciones
- cero locks
- cero deficit
- dos vaults independientes

## `basic`

Alice deposita en `hxSOL`, bloquea una parte de sus shares, pide una redencion
parcial, el vault liquida el epoch y Alice retira los assets.

Propiedades esperadas:

- un lock abierto con shares remanentes
- una redencion liquidada y retirada
- `quotes_ok = true`
- reserves restantes iguales a live shares mas locked shares

## `multi`

Tres usuarios operan dos vaults. `hxUSD` cambia de indice antes de que Bob
bloquee shares. Bob transfiere una parte a Carol en el mismo epoch de apertura
del lock, por lo que el lock hijo no refresca indice.

Propiedades esperadas:

- epochs independientes por vault
- redenciones con indices correctos
- positions separadas por `vault_id`
- ninguna insolvencia

## `index`

El vault cambia de indice con live shares activas, recibe un segundo deposito,
crea un lock y liquida una redencion parcial. Despues vuelve a cambiar el
indice. El escenario comprueba que solo las live shares capturan la
revalorizacion posterior.

Propiedades esperadas:

- `share_index_text = "1.400000"` al final
- la redencion se cotiza al indice de lock `"1.250000"`
- no hay quote por encima de `issued_index`

## `partial`

Alice bloquea una posicion grande y solicita dos redenciones parciales en
epochs diferentes. El lock queda abierto con shares restantes.

Propiedades esperadas:

- dos redenciones retiradas
- un lock abierto
- `retired_shares` igual a la suma redimida
- reserves cubren live shares mas locked shares restantes

## `transfer`

Alice transfiere locked shares a Bob antes de avanzar el epoch. Bob redime el
lock hijo despues del avance. Esta ruta es segura porque el lock hijo hereda el
indice original antes del cambio de epoch.

Propiedades esperadas:

- `index_refreshed = false`
- `quote_index = issued_index`
- Bob recibe exactamente el valor esperado

## `drift`

Escenario de diagnostico que conserva observable la vulnerabilidad del CTF. No
forma parte de los tests de comportamiento sano. Su proposito es que un autor de
mitigacion pueda confirmar que el bug existe antes de corregirlo.

Propiedades observables:

- lock hijo con `index_refreshed = true`
- redencion con `assets > expected_assets`
- `totals.excess_quote_assets > 0`
- posible deficit tras settlement

## Como usar los escenarios al mitigar

1. Ejecuta `npm test` y confirma que los escenarios sanos pasan.
2. Ejecuta `build/helixdtl drift` y observa el exceso de quote.
3. Corrige la regla de transferencia de locked shares.
4. Anade un test de regresion que espere que la ruta de drift ya no produzca
   exceso.
5. Repite `npm test`.

Una mitigacion que elimina el exceso pero rompe `basic`, `multi`, `index`,
`partial` o `transfer` no conserva la semantica esperada del protocolo.
