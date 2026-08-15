# Arquitectura

## Objetivo Del Sistema

HelixDTL separa el motor economico de la capa de integracion. El proceso C11 es
la autoridad del estado; el CLI traduce comandos, y el cliente Node.js valida y
agrega las respuestas sin mutarlas. Esta separacion permite reproducir una
secuencia con el mismo resultado en Windows y Linux.

```mermaid
flowchart TB
    subgraph Integracion
        APP["Servicio de tesoreria"]
        SDK["HelixClient"]
    end
    subgraph Ejecucion
        CLI["main + script"]
        LED["ledger"]
        QTE["quote"]
        RSK["risk"]
    end
    subgraph Evidencia
        COD["codec JSON"]
        INV["invariants"]
        DIG["state digest"]
    end
    APP --> SDK --> CLI
    CLI --> LED
    CLI --> QTE
    LED --> INV
    LED --> COD
    RSK --> APP
    INV --> COD --> DIG --> SDK
```

## Dominios De Estado

`HlxLedger` contiene arrays de capacidad fija. No existen punteros persistentes,
asignacion dinamica ni I/O dentro de las reglas economicas. Cada entidad incluye
un indicador `active`; los ids se asignan de forma monotona y nunca se reutilizan
durante la vida del proceso.

```mermaid
classDiagram
    class HlxLedger
    class HlxAccount {
        id
        external_assets
        withdrawn_assets
    }
    class HlxVault {
        epoch
        share_index
        reserves
        live_shares
        pending_assets
    }
    class HlxPosition {
        account_id
        vault_id
        liquid_shares
    }
    class HlxLock {
        owner_id
        parent_lock_id
        shares
        issued_index
        redemption_index
    }
    class HlxRedemption {
        lock_id
        shares
        assets
        settled
        withdrawn
    }
    HlxLedger "1" *-- "many" HlxAccount
    HlxLedger "1" *-- "many" HlxVault
    HlxAccount "1" --> "many" HlxPosition
    HlxVault "1" --> "many" HlxPosition
    HlxPosition "1" --> "many" HlxLock
    HlxLock "1" --> "many" HlxRedemption
```

## Ruta De Una Orden

El parser valida aridad y tipos antes de llamar a la API. La API vuelve a
validar existencia, propiedad, capacidad y cantidades. Solo despues actualiza
las entidades relacionadas y emite un evento.

```mermaid
sequenceDiagram
    participant F as Archivo .hlx
    participant P as Parser
    participant A as API del ledger
    participant E as Entidades
    participant J as Codec JSON
    F->>P: linea y numero
    P->>P: tokeniza y valida tipos
    P->>A: llamada tipada
    A->>A: valida dominio
    A->>E: mutacion atomica
    A->>E: registra evento
    F->>P: snapshot
    P->>J: serializa ledger
    J-->>F: JSON + digest
```

## Limites

Los limites (`HLX_MAX_*`) forman parte del contrato. Una operacion que agotaria
una tabla devuelve `HLX_ERR_CAPACITY` y no debe reintentarse sin rotar el
proceso o compactar el estado en una version futura. Los labels y simbolos se
truncan a su tamano maximo mediante escritura acotada.

## Extensibilidad

Nuevos comandos deben seguir cuatro pasos: declarar la funcion en el header,
implementar una transicion autocontenida, exponerla en `script.c` y agregar
pruebas de caso valido, rechazo y snapshot. Una vista observacional no debe
escribir en `HlxLedger`.
