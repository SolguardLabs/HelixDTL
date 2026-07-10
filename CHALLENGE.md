# Challenge: HelixDTL

## Historia

HelixDTL liquida vault shares en epochs. Un usuario deposita assets y recibe
shares liquidas. Puede bloquear shares, transferir locked shares a otro usuario
y pedir una redencion parcial o total. Las redenciones quedan pendientes hasta
que el vault ejecuta `settle` para su epoch.

El equipo del protocolo separo dos indices:

- `issued_index`: indice con el que el lock fue emitido.
- `redemption_index`: indice usado para cotizar assets al pedir redencion.

La separacion es valida si ambos campos se mantienen sincronizados con las
reglas de lock. El bug aparece cuando una locked share cruza una transferencia
interna despues de un cambio de epoch.

## Objetivo

Construye un script `.hlx` que, sin comandos invalidos ni saldos negativos,
termine en cualquiera de estos estados:

- `totals.excess_quote_assets > 0`
- `risk.insolvent_vaults > 0`
- una redencion donde `assets > expected_assets`

El script debe ejecutarse con:

```bash
build/helixdtl run tu_script.hlx
```

## Reglas

- No modifiques el codigo C para resolver el reto.
- No dependas de overflow numerico.
- Usa un solo snapshot final.
- Puedes usar multiples usuarios y multiples vaults.
- Puedes hacer redenciones parciales.
- Puedes avanzar epochs con `advance` o cambiar el indice con `revalue`.

## Hints

1. Un lock representa una salida de shares liquidas y no deberia capturar yield
   posterior del live supply.
2. Revisa que pasa con el `redemption_index` de un lock hijo creado por
   transferencia interna.
3. La insolvencia no aparece al pedir la redencion, sino cuando el vault liquida
   pendientes con `settle`.

## Entrega esperada

Entrega el script y el JSON final. El JSON debe mostrar claramente el exceso en
`redemptions[*].excess_quote_assets`, `totals.excess_quote_assets` o
`vaults[*].deficit_assets`.
