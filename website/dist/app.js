const SITE_DATA = {"address": "Galata slope, Beyoglu / Istanbul", "beans": [{"name": "Halo", "notes": "jasmine, peach, bergamot", "origin": "Ethiopia", "roast": "light"}, {"name": "Sierra", "notes": "cacao, cherry, panela", "origin": "Colombia", "roast": "medium"}, {"name": "Tufa", "notes": "blackcurrant, citrus, syrup", "origin": "Kenya", "roast": "light"}], "city": "Istanbul", "hero_copy": "North Ember serves textured espresso, bright filter brews, and low-light evenings built for conversation.", "hero_title": "A neighborhood coffee room with a warm pulse.", "hours": "Mon-Sun / 08:00 - 23:00", "journal": [{"text": "Analog soul, ambient jazz, and soft-volume sets from 20:00 to close.", "title": "Friday listening sets"}, {"text": "Every Tuesday at 09:00 we open the brew bar for comparative pourovers.", "title": "Morning brew lab"}, {"text": "Burnt vanilla bun, olive oil cake, and sesame shortbread rotate weekly.", "title": "House pastry drop"}], "phone": "+90 212 555 01 77", "primary_cta": "Book a tasting table", "secondary_cta": "See the room", "signature_drinks": [{"accent": "smoke", "name": "Cinder Latte", "price": 140, "profile": "espresso, dark honey, velvet milk"}, {"accent": "bright", "name": "Apricot Tonic", "price": 150, "profile": "cold brew, tonic, dried apricot"}, {"accent": "green", "name": "Forest Flat", "price": 145, "profile": "espresso, pine syrup, microfoam"}, {"accent": "floral", "name": "Night Filter", "price": 135, "profile": "washed ethiopia, jasmine finish"}], "site_name": "North Ember", "story_copy": "The room opens with washed oak, rust lacquer, and daylight in the morning, then turns into an amber listening corner after sunset.", "story_points": ["Seasonal espresso flights every Friday", "Single-origin wall refreshed every two weeks", "Quiet daytime tables with power and fast wifi"], "story_title": "Designed like a listening bar, brewed like a roastery counter.", "tagline": "Slow coffee, bright rooms, late records."};
const GENERATED_AT = "2026-03-23T14:17:25Z";
const byId = (id) => document.getElementById(id);
const money = (value) => `TRY ${value}`;
byId('site-name').textContent = SITE_DATA.site_name;
byId('tagline').textContent = SITE_DATA.tagline;
byId('hero-title').textContent = SITE_DATA.hero_title;
byId('hero-copy').textContent = SITE_DATA.hero_copy;
byId('primary-cta').textContent = SITE_DATA.primary_cta;
byId('secondary-cta').textContent = SITE_DATA.secondary_cta;
byId('city-line').textContent = `${SITE_DATA.city} / ${SITE_DATA.hours}`;
byId('story-title').textContent = SITE_DATA.story_title;
byId('story-copy').textContent = SITE_DATA.story_copy;
byId('hours').textContent = SITE_DATA.hours;
byId('address').textContent = SITE_DATA.address;
byId('phone').textContent = SITE_DATA.phone;
byId('generated-at').textContent = GENERATED_AT;
byId('footer-city').textContent = SITE_DATA.city;
const storyMarkup = SITE_DATA.story_points.map((point) => `<li>${point}</li>`).join('');
byId('story-points').innerHTML = storyMarkup;
const signatureMarkup = SITE_DATA.signature_drinks.map((drink) => `
  <article class='menu-card'>
    <span class='accent-pill'>${drink.accent}</span>
    <h4>${drink.name}</h4>
    <p>${drink.profile}</p>
    <div class='price-line'>${money(drink.price)}</div>
  </article>
`).join('');
byId('signature-grid').innerHTML = signatureMarkup;
const beanMarkup = SITE_DATA.beans.map((bean) => `
  <article class='bean-card'>
    <span class='accent-pill'>${bean.roast}</span>
    <h4>${bean.name}</h4>
    <p>${bean.origin}</p>
    <p>${bean.notes}</p>
  </article>
`).join('');
byId('bean-grid').innerHTML = beanMarkup;
const journalMarkup = SITE_DATA.journal.map((entry) => `
  <article class='journal-card'>
    <span class='accent-pill'>house note</span>
    <h4>${entry.title}</h4>
    <p>${entry.text}</p>
  </article>
`).join('');
byId('journal-grid').innerHTML = journalMarkup;
